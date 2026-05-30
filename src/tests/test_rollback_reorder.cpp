// Out-of-order delivery is handled idempotently.
//
// JitterTransport's per-packet random delay naturally lets a later-sent
// packet arrive ahead of an earlier-sent one. The receiver must apply
// each batch entry to the input ring via injectRemoteInput, where the
// stale-frame drop (frame <= confirmedSimFrame_) and idempotent write
// (same byte each time) keep state consistent regardless of arrival
// order. This test cranks up the delay range to maximise reordering and
// adds duplication on top.
//
// Verifies that despite the out-of-order arrivals:
//   - Both peers reach the same simFrame within the test horizon.
//   - Final state checksums match.
//   - Rollback actually fires (so we're not testing a vacuous fast path).

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <memory>

#include "controller/rollbackController.hpp"
#include "game.hpp"
#include "jitter_transport.hpp"
#include "math.hpp"
#include "mixer/player.hpp"
#include "rollback/buffer.hpp"

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

TEST_CASE("Rollback survives reorder + duplication", "[rollback][reorder]") {
  constexpr uint32_t kWorldSeed = 0xBEEF;
  constexpr int kTicks = 1000;
  constexpr uint32_t kInputSeed = 0xC0FFEE;

  auto [common, settings] = makeEnv();
  auto a = std::make_unique<RollbackController>(common, settings, 0);
  auto b = std::make_unique<RollbackController>(common, settings, 1);
  a->setSkipWeaponSelection(true);
  b->setSkipWeaponSelection(true);
  // Frame-advantage stall clamps the peers to lockstep and
  // suppresses the prediction window we need to exercise the reorder
  // path — disable it so we measure the dedup mechanism directly.
  a->setFrameAdvantageEnabled(false);
  b->setFrameAdvantageEnabled(false);
  a->game.rand.seed(kWorldSeed);
  b->game.rand.seed(kWorldSeed);

  // Wide delay band + duplication. With min=1, max=5 the variance is
  // large enough that earlier-sent packets routinely arrive after
  // later-sent ones; 30% duplication exercises the idempotent overwrite
  // path on remoteInputs slots and the stale-frame drop inside
  // injectRemoteInput. No loss here — that's covered by the loss test.
  rollback_test::JitterTransport transport(
      {0xACE1, /*minDelay*/ 1, /*maxDelay*/ 5,
       /*lossProb*/ 0.0, /*dupProb*/ 0.30});

  a->setInputCallbacks(
      [&](uint8_t gen_, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
        transport.sendAToB(gen_, bf, c, in, lf);
      });
  b->setInputCallbacks(
      [&](uint8_t gen_, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
        transport.sendBToA(gen_, bf, c, in, lf);
      });
  a->focus();
  b->focus();

  for (uint32_t f = 0; f < 3; ++f) {
    a->injectRemoteInput(f, 0);
    b->injectRemoteInput(f, 0);
  }

  auto deliverA = [&](uint8_t gen_, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
    a->injectRemoteBatch(bf, c, in, lf);
  };
  auto deliverB = [&](uint8_t gen_, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
    b->injectRemoteBatch(bf, c, in, lf);
  };

  Rand inputRng(kInputSeed);
  for (int tick = 0; tick < kTicks; ++tick) {
    uint8_t inA = inputRng() & 0x7f;
    uint8_t inB = inputRng() & 0x7f;
    if ((inputRng() % 10) < 6) inA |= (1 << Worm::Fire);
    if ((inputRng() % 10) < 6) inB |= (1 << Worm::Fire);
    a->setLocalControlState(inA);
    b->setLocalControlState(inB);
    a->process();
    b->process();
    transport.tick(deliverA, deliverB);
  }

  transport.flush(deliverA, deliverB);
  a->setLocalControlState(0);
  b->setLocalControlState(0);
  for (int i = 0; i < 16; ++i) {
    a->process();
    b->process();
    transport.tick(deliverA, deliverB);
  }

  REQUIRE(transport.packetsDuplicated > 0);
  REQUIRE(a->rollbackCount() > 0);
  REQUIRE(b->rollbackCount() > 0);
  REQUIRE(a->currentFrame() == b->currentFrame());
  REQUIRE(wideRollbackChecksum(a->game) == wideRollbackChecksum(b->game));
}
