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

std::pair<std::shared_ptr<Common>, std::shared_ptr<Settings>> makeEnv() {
  precomputeTables();
  auto common = std::make_shared<Common>();
  FsNode tcRoot(FsNode("data") / "TC" / "openliero");
  common->load(std::move(tcRoot));
  auto settings = std::make_shared<Settings>();
  settings->lives = 10;
  settings->loadingTime = 0;
  settings->loadChange = true;
  settings->randomLevel = true;
  settings->gameMode = Settings::GMKillEmAll;
  return {common, settings};
}

}  // namespace

TEST_CASE("Rollback controller drops batches from an older generation",
          "[rollback][generation]") {
  auto [common, settings] = makeEnv();
  RollbackController a(common, settings, 0);
  a.setSkipWeaponSelection(true);
  a.game.rand.seed(0xC0FFEE);
  a.focus();

  // Put the controller into the post-transition state directly
  // (production code crosses this via finishWeaponSelect).
  a.setGenerationForTest(1);
  REQUIRE(a.generation() == 1);
  REQUIRE(a.droppedOldGenerationBatches() == 0);

  // Build a stale-generation batch covering frames [0, 7]. With kInputDelay
  // = 1 (rollback default), confirmedFrame() starts at -1 and the ring
  // slots for frames 0..7 are the ones the live game phase is about to
  // use — accepting these would have remoteInputReady[i] = true with a
  // value picked from a previous phase.
  uint8_t inputs[8] = {0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8};

  SECTION("older generation is dropped") {
    a.injectRemoteBatch(/*generation=*/0, /*baseFrame=*/0, /*count=*/8,
                        inputs, /*remoteLocalFrame=*/3);

    REQUIRE(a.droppedOldGenerationBatches() == 1);
    // lastKnownRemoteFrame_ stays at its sentinel — a dropped packet
    // must not advance the frame-advantage estimate either.
    REQUIRE(a.lastKnownRemoteFrame() == -1);
    // confirmedFrame is unchanged.
    REQUIRE(a.confirmedFrame() == -1);
  }

  SECTION("matching generation is accepted") {
    a.injectRemoteBatch(/*generation=*/1, /*baseFrame=*/0, /*count=*/8,
                        inputs, /*remoteLocalFrame=*/3);

    REQUIRE(a.droppedOldGenerationBatches() == 0);
    REQUIRE(a.lastKnownRemoteFrame() == 3);
  }

  SECTION("immediate-future generation is buffered") {
    // gen+1 batches are buffered until resetForGamePhase bumps
    // generation_. Until then no input is applied and the drop counter
    // doesn't bump.
    a.injectRemoteBatch(/*generation=*/2, /*baseFrame=*/0, /*count=*/8,
                        inputs, /*remoteLocalFrame=*/3);

    REQUIRE(a.droppedOldGenerationBatches() == 0);
    REQUIRE(a.pendingFutureBatchCount() == 1);
    REQUIRE(a.lastKnownRemoteFrame() == -1);
  }

  SECTION("far-future generation (gen+2 or more) is dropped") {
    // Beyond one phase ahead, the packet describes a simFrame numbering
    // we may never reach in this match. Drop conservatively.
    a.injectRemoteBatch(/*generation=*/3, /*baseFrame=*/0, /*count=*/8,
                        inputs, /*remoteLocalFrame=*/3);

    REQUIRE(a.droppedOldGenerationBatches() == 1);
    REQUIRE(a.pendingFutureBatchCount() == 0);
    REQUIRE(a.lastKnownRemoteFrame() == -1);
  }
}

// resetForGamePhase must clear every piece of state so the post-bump
// game phase starts from a known-empty baseline.
TEST_CASE("resetForGamePhase clears controller state and bumps generation",
          "[rollback][generation]") {
  auto [common, settings] = makeEnv();
  RollbackController c(common, settings, 0);
  c.setSkipWeaponSelection(true);
  c.game.rand.seed(0xC0FFEE);
  c.focus();

  // Drive a handful of frames so simFrame, confirmedFrame, the rollback
  // ring, lastKnownRemoteFrame, etc. are non-default. Seed remote inputs
  // for the input-delay window so frames advance as confirmed.
  for (uint32_t f = 0; f < 16; ++f) c.injectRemoteInput(f, 0);
  for (int i = 0; i < 10; ++i) {
    c.setLocalControlState(0);
    c.process();
  }
  // Also feed a batched packet so lastKnownRemoteFrame advances.
  uint8_t bytes[2] = {0, 0};
  c.injectRemoteBatch(/*generation=*/0, /*baseFrame=*/8, /*count=*/2, bytes,
                      /*remoteLocalFrame=*/9);

  REQUIRE(c.currentFrame() > 0);
  REQUIRE(c.confirmedFrame() >= 0);
  REQUIRE(c.lastKnownRemoteFrame() >= 0);
  REQUIRE_FALSE(c.rollbackBuffer().empty());
  uint8_t prevGen = c.generation();

  c.resetForGamePhaseForTest();

  REQUIRE(c.currentFrame() == 0);
  REQUIRE(c.confirmedFrame() == -1);
  REQUIRE(c.lastKnownRemoteFrame() == -1);
  REQUIRE(c.lastTickResimFrames() == 0);
  REQUIRE(c.generation() == static_cast<uint8_t>(prevGen + 1));
  // Ring is empty: no slot can be looked up by frame, oldest/newest
  // sentinel back to -1.
  REQUIRE(c.rollbackBuffer().empty());
  REQUIRE(c.rollbackBuffer().newestFrame() == -1);
  REQUIRE(c.rollbackBuffer().oldestFrame() == -1);

  // And the post-reset filter accepts gen-1 (= prevGen+1) batches but
  // drops gen-prevGen ones — proving the new generation took effect.
  c.injectRemoteBatch(prevGen, /*baseFrame=*/0, /*count=*/2, bytes,
                      /*remoteLocalFrame=*/1);
  REQUIRE(c.droppedOldGenerationBatches() >= 1);
  REQUIRE(c.confirmedFrame() == -1);

  c.injectRemoteBatch(static_cast<uint8_t>(prevGen + 1), /*baseFrame=*/0,
                      /*count=*/2, bytes, /*remoteLocalFrame=*/1);
  REQUIRE(c.lastKnownRemoteFrame() == 1);
}
