// Generation drop.
//
// A controller that has crossed the WS→game generation bump must ignore
// any batch carrying the old (pre-transition) generation. The frame
// numbers in such a batch belong to the abandoned simFrame numbering
// and, if accepted, would corrupt a live game-phase slot — the ring
// buffer reuses the same frame-modulo slots after the reset, so an
// inbound stale frame N would re-mark a freshly written slot N as
// having a real remote input from the old phase.

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <memory>

#include "controller/rollbackController.hpp"
#include "game.hpp"
#include "math.hpp"

namespace {

std::pair<std::shared_ptr<Common>, std::shared_ptr<Settings>> MakeEnv() {
  PrecomputeTables();
  auto common = std::make_shared<Common>();
  FsNode const kTcRoot(FsNode("data") / "TC" / "openliero");
  common->load(kTcRoot);
  auto settings = std::make_shared<Settings>();
  settings->lives = 10;
  settings->loading_time = 0;
  settings->load_change = true;
  settings->random_level = true;
  settings->game_mode = Settings::kGmKillEmAll;
  return {common, settings};
}

}  // namespace

TEST_CASE("Rollback controller drops batches from an older generation", "[rollback][generation]") {
  auto [common, settings] = MakeEnv();
  RollbackController a(common, settings, 0);
  a.SetSkipWeaponSelection(/*skip=*/true);
  a.game.rand.Seed(0xC0FFEE);
  a.Focus();

  // Put the controller into the post-transition state directly
  // (production code crosses this via finishWeaponSelect).
  a.SetGenerationForTest(1);
  REQUIRE(a.Generation() == 1);
  REQUIRE(a.DroppedOldGenerationBatches() == 0);

  // Build a stale-generation batch covering frames [0, 7]. With kInputDelay
  // = 1 (rollback default), confirmedFrame() starts at -1 and the ring
  // slots for frames 0..7 are the ones the live game phase is about to
  // use — accepting these would have remoteInputReady[i] = true with a
  // value picked from a previous phase.
  uint8_t inputs[8] = {0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8};

  SECTION("older generation is dropped") {
    a.InjectRemoteBatch(/*generation=*/0, /*base_frame=*/0, /*count=*/8, inputs,
                        /*remote_local_frame=*/3);

    REQUIRE(a.DroppedOldGenerationBatches() == 1);
    // lastKnownRemoteFrame_ stays at its sentinel — a dropped packet
    // must not advance the frame-advantage estimate either.
    REQUIRE(a.LastKnownRemoteFrame() == -1);
    // confirmedFrame is unchanged.
    REQUIRE(a.ConfirmedFrame() == -1);
  }

  SECTION("matching generation is accepted") {
    a.InjectRemoteBatch(/*generation=*/1, /*base_frame=*/0, /*count=*/8, inputs,
                        /*remote_local_frame=*/3);

    REQUIRE(a.DroppedOldGenerationBatches() == 0);
    REQUIRE(a.LastKnownRemoteFrame() == 3);
  }

  SECTION("immediate-future generation is buffered") {
    // gen+1 batches are buffered until resetForGamePhase bumps
    // generation_. Until then no input is applied and the drop counter
    // doesn't bump.
    a.InjectRemoteBatch(/*generation=*/2, /*base_frame=*/0, /*count=*/8, inputs,
                        /*remote_local_frame=*/3);

    REQUIRE(a.DroppedOldGenerationBatches() == 0);
    REQUIRE(a.PendingFutureBatchCount() == 1);
    REQUIRE(a.LastKnownRemoteFrame() == -1);
  }

  SECTION("far-future generation (gen+2 or more) is dropped") {
    // Beyond one phase ahead, the packet describes a simFrame numbering
    // we may never reach in this match. Drop conservatively.
    a.InjectRemoteBatch(/*generation=*/3, /*base_frame=*/0, /*count=*/8, inputs,
                        /*remote_local_frame=*/3);

    REQUIRE(a.DroppedOldGenerationBatches() == 1);
    REQUIRE(a.PendingFutureBatchCount() == 0);
    REQUIRE(a.LastKnownRemoteFrame() == -1);
  }
}

// resetForGamePhase must clear every piece of state so the post-bump
// game phase starts from a known-empty baseline.
TEST_CASE("resetForGamePhase clears controller state and bumps generation",
          "[rollback][generation]") {
  auto [common, settings] = MakeEnv();
  RollbackController c(common, settings, 0);
  c.SetSkipWeaponSelection(/*skip=*/true);
  c.game.rand.Seed(0xC0FFEE);
  c.Focus();

  // Drive a handful of frames so simFrame, confirmedFrame, the rollback
  // ring, lastKnownRemoteFrame, etc. are non-default. Seed remote inputs
  // for the input-delay window so frames advance as confirmed.
  for (uint32_t f = 0; f < 16; ++f) c.InjectRemoteInput(f, 0);
  for (int i = 0; i < 10; ++i) {
    c.SetLocalControlState(0);
    c.Process();
  }
  // Also feed a batched packet so lastKnownRemoteFrame advances.
  uint8_t bytes[2] = {0, 0};
  c.InjectRemoteBatch(/*generation=*/0, /*base_frame=*/8, /*count=*/2, bytes,
                      /*remote_local_frame=*/9);

  REQUIRE(c.CurrentFrame() > 0);
  REQUIRE(c.ConfirmedFrame() >= 0);
  REQUIRE(c.LastKnownRemoteFrame() >= 0);
  REQUIRE_FALSE(c.RollbackBuffer().Empty());
  uint8_t const kPrevGen = c.Generation();

  c.ResetForGamePhaseForTest();

  REQUIRE(c.CurrentFrame() == 0);
  REQUIRE(c.ConfirmedFrame() == -1);
  REQUIRE(c.LastKnownRemoteFrame() == -1);
  REQUIRE(c.LastTickResimFrames() == 0);
  REQUIRE(c.Generation() == static_cast<uint8_t>(kPrevGen + 1));
  // Ring is empty: no slot can be looked up by frame, oldest/newest
  // sentinel back to -1.
  REQUIRE(c.RollbackBuffer().Empty());
  REQUIRE(c.RollbackBuffer().NewestFrame() == -1);
  REQUIRE(c.RollbackBuffer().OldestFrame() == -1);

  // And the post-reset filter accepts gen-1 (= prevGen+1) batches but
  // drops gen-prevGen ones — proving the new generation took effect.
  c.InjectRemoteBatch(kPrevGen, /*base_frame=*/0, /*count=*/2, bytes,
                      /*remote_local_frame=*/1);
  REQUIRE(c.DroppedOldGenerationBatches() >= 1);
  REQUIRE(c.ConfirmedFrame() == -1);

  c.InjectRemoteBatch(static_cast<uint8_t>(kPrevGen + 1), /*base_frame=*/0,
                      /*count=*/2, bytes, /*remote_local_frame=*/1);
  REQUIRE(c.LastKnownRemoteFrame() == 1);
}
