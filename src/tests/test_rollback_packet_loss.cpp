// Packet loss survives via input redundancy.
//
// The wire format carries the last K = kMaxRollback + 1 inputs per
// packet, so a single dropped packet is covered by the next K-1
// packets. Under steady-state moderate loss the controllers should
// never stall: every frame eventually receives confirmed input within
// kMaxRollback frames of being run as a prediction.
//
// Setup: two RollbackControllers wired through JitterTransport with
// lossProbability = 0.10 and a small delay floor (so reorder is
// possible). Drive random inputs, then assert (a) both peers advance
// past a reasonable horizon, (b) the steady-state confirmation lag
// stays bounded by kMaxRollback, and (c) checksums agree at the end.

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <memory>
#include <vector>

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

TEST_CASE("Rollback survives 10% packet loss via input redundancy",
          "[rollback][loss]") {
  constexpr uint32_t kWorldSeed = 0xBEEF;
  constexpr int kTicks = 1500;
  constexpr uint32_t kInputSeed = 0xC0FFEE;

  auto [common, settings] = makeEnv();
  auto a = std::make_unique<RollbackController>(common, settings, 0);
  auto b = std::make_unique<RollbackController>(common, settings, 1);
  a->setSkipWeaponSelection(true);
  b->setSkipWeaponSelection(true);
  // Frame-advantage stall is orthogonal to packet loss; turn it off so
  // the peers freely run ahead and we measure redundancy in isolation.
  a->setFrameAdvantageEnabled(false);
  b->setFrameAdvantageEnabled(false);
  a->game.rand.seed(kWorldSeed);
  b->game.rand.seed(kWorldSeed);

  rollback_test::JitterTransport transport(
      {0x10ADED, /*minDelay*/ 1, /*maxDelay*/ 3,
       /*lossProb*/ 0.10, /*dupProb*/ 0.0});

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
  uint32_t maxLagA = 0;
  uint32_t maxLagB = 0;
  int stallTicks = 0;
  uint32_t prevA = 0;
  uint32_t prevB = 0;

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

    // Steady-state observation begins after the first ~50 ticks so the
    // warm-up doesn't pollute the running maxima.
    if (tick > 50) {
      uint32_t lagA = a->currentFrame() -
                     static_cast<uint32_t>(a->confirmedFrame() + 1);
      uint32_t lagB = b->currentFrame() -
                     static_cast<uint32_t>(b->confirmedFrame() + 1);
      if (lagA > maxLagA) maxLagA = lagA;
      if (lagB > maxLagB) maxLagB = lagB;
      if (a->currentFrame() == prevA && b->currentFrame() == prevB)
        ++stallTicks;
    }
    prevA = a->currentFrame();
    prevB = b->currentFrame();
  }

  // Steady-state lag never reaches the stall threshold (kMaxRollback+1).
  // With K-wide redundancy a single dropped packet is covered by the
  // next packet, so the lag tops out at the natural network delay
  // plus a handful of redundant-covered drops.
  REQUIRE(maxLagA <= static_cast<uint32_t>(rollback::kMaxRollback));
  REQUIRE(maxLagB <= static_cast<uint32_t>(rollback::kMaxRollback));

  // The pair never enters a cascading stall — at least one peer
  // advances every tick under 10% loss.
  REQUIRE(stallTicks == 0);

  // Loss should actually have fired; otherwise the test is vacuous.
  REQUIRE(transport.packetsDropped > 0);
  REQUIRE(a->rollbackCount() > 0);
  REQUIRE(b->rollbackCount() > 0);

  // Flush and drain, then assert checksums agree.
  transport.flush(deliverA, deliverB);
  a->setLocalControlState(0);
  b->setLocalControlState(0);
  for (int i = 0; i < 16; ++i) {
    a->process();
    b->process();
    transport.tick(deliverA, deliverB);
  }
  REQUIRE(a->currentFrame() == b->currentFrame());
  REQUIRE(wideRollbackChecksum(a->game) == wideRollbackChecksum(b->game));
}
