// Future-generation packet buffering.
//
// When peer A transitions from WS to game phase before peer B, A sends
// generation-1 batches that B receives while still at generation 0.
// Pure-drop would rely on the K-wide redundancy to resend the boundary
// frames after B catches up; we instead buffer up to kMaxRollback+1
// future-gen batches so the first game-phase frames are guaranteed to
// arrive intact. Anything older than the current generation, or further
// future than gen+1, is still dropped — those are unrecoverable.

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <memory>

#include "controller/rollbackController.hpp"
#include "game.hpp"
#include "math.hpp"
#include "rollback/buffer.hpp"

namespace {

std::pair<std::shared_ptr<Common>, std::shared_ptr<Settings>> MakeEnv() {
  PrecomputeTables();
  auto common = std::make_shared<Common>();
  FsNode tc_root(FsNode("data") / "TC" / "openliero");
  common->load(std::move(tc_root));
  auto settings = std::make_shared<Settings>();
  settings->lives = 10;
  settings->loading_time = 0;
  settings->load_change = true;
  settings->random_level = true;
  settings->game_mode = Settings::kGmKillEmAll;
  return {common, settings};
}

}  // namespace

TEST_CASE("Future-generation batches buffer and drain on reset", "[rollback][generation]") {
  auto [common, settings] = MakeEnv();
  RollbackController c(common, settings, 0);
  c.SetSkipWeaponSelection(true);
  c.game.rand.Seed(0xC0FFEE);
  c.Focus();

  REQUIRE(c.Generation() == 0);
  REQUIRE(c.PendingFutureBatchCount() == 0);

  // Peer is ~5 game-phase frames ahead — sends generation-1 batches
  // covering its frames [0..7], [1..8], ... while we're still at gen 0.
  // The buffer holds one window's worth (= one full K-wide batch).
  uint8_t inputs[8] = {0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7};
  c.InjectRemoteBatch(/*generation=*/1, /*baseFrame=*/0, /*count=*/8, inputs,
                      /*remoteLocalFrame=*/5);

  // Not applied yet — generation_ is still 0 so the wire layer holds
  // the batch. The drop counter must NOT increment (this isn't a drop).
  REQUIRE(c.PendingFutureBatchCount() == 1);
  REQUIRE(c.DroppedOldGenerationBatches() == 0);
  REQUIRE(c.LastKnownRemoteFrame() == -1);

  // Far-future packets ARE dropped (no buffer slot, no recovery path).
  c.InjectRemoteBatch(/*generation=*/2, /*baseFrame=*/0, /*count=*/8, inputs,
                      /*remoteLocalFrame=*/5);
  REQUIRE(c.PendingFutureBatchCount() == 1);
  REQUIRE(c.DroppedOldGenerationBatches() == 1);

  // Now we transition. resetForGamePhase bumps generation_ to 1, then
  // drains the buffered batch through injectRemoteBatch as if it had
  // just arrived.
  c.ResetForGamePhaseForTest();

  REQUIRE(c.Generation() == 1);
  REQUIRE(c.PendingFutureBatchCount() == 0);
  // The drained batch advances lastKnownRemoteFrame_ — proves the
  // batch was actually replayed through the normal receive path, not
  // just discarded.
  REQUIRE(c.LastKnownRemoteFrame() == 5);
  // confirmedSimFrame_ stays at -1: no local frame has been advanced
  // yet, so even though remote inputs are present the local can't have
  // "confirmed" them — the new-frame block in advanceSimulation will
  // consume them on the next process() tick.
  REQUIRE(c.ConfirmedFrame() == -1);
}

// Bounded buffer: only kMaxRollback+1 entries fit. Beyond that the
// receiver falls through to the drop counter. With the K-wide
// redundancy, a single overflowed entry rarely loses unique frames —
// the next batch usually contains the same window — but this test
// pins the cap so a future enlarged buffer doesn't silently grow.
TEST_CASE("Future-generation buffer is bounded", "[rollback][generation]") {
  auto [common, settings] = MakeEnv();
  RollbackController c(common, settings, 0);
  c.SetSkipWeaponSelection(true);
  c.game.rand.Seed(0xBEEF);
  c.Focus();

  uint8_t inputs[8] = {0};

  constexpr int kCap = rollback::kMaxRollback + 1;
  for (int i = 0; i < kCap; ++i) {
    c.InjectRemoteBatch(/*generation=*/1,
                        /*baseFrame=*/static_cast<uint32_t>(i),
                        /*count=*/1, inputs,
                        /*remoteLocalFrame=*/static_cast<uint32_t>(i));
  }
  REQUIRE(c.PendingFutureBatchCount() == kCap);
  REQUIRE(c.DroppedOldGenerationBatches() == 0);

  // One past the cap: the buffer is full, falls through, drop counter
  // bumps but the buffer doesn't grow.
  c.InjectRemoteBatch(/*generation=*/1, /*baseFrame=*/100, /*count=*/1, inputs,
                      /*remoteLocalFrame=*/100);
  REQUIRE(c.PendingFutureBatchCount() == kCap);
  REQUIRE(c.DroppedOldGenerationBatches() == 1);
}
