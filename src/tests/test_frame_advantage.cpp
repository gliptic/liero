// Frame-advantage / time-sync stall.
//
// Each batched packet carries the sender's simFrame. The receiver
// tracks the largest such value; when its own simFrame is at least
// kFrameAdvantage frames past that estimate it holds for one tick (the
// redundant send still fires — only the sim step is held).
//
// Under wildly asymmetric one-way delays the algorithm settles to a
// steady state where both peers' simFrames stay tightly coupled. This
// test drives D_AB = 2, D_BA = 6 and asserts the gap stays bounded.

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

// Run one peer-pair with the given fixed one-way delays (D_AB, D_BA)
// and return the maximum |simFrame_A - simFrame_B| observed during the
// post-warm-up window. Also surfaces stall counts so the assertion can
// guard against a vacuously-passing test (zero stalls would mean the
// delays were symmetric enough that the threshold never fired).
struct AsymmetricRun {
  int maxAbsGap;
  uint64_t stallsA;
  uint64_t stallsB;
};

AsymmetricRun runAsymmetric(int dAB, int dBA, int ticks) {
  auto [common, settings] = makeEnv();
  auto a = std::make_unique<RollbackController>(common, settings, 0);
  auto b = std::make_unique<RollbackController>(common, settings, 1);
  a->setSkipWeaponSelection(true);
  b->setSkipWeaponSelection(true);
  a->game.rand.seed(0xBEEF);
  b->game.rand.seed(0xBEEF);

  // Asymmetric one-way delays modelled by two separate "transports":
  // one for A→B (fixed delay dAB), one for B→A (fixed delay dBA).
  // Loss/dup/jitter are off so the test isolates the time-sync feedback.
  rollback_test::JitterTransport tAB({0x1111, dAB, dAB, 0.0, 0.0});
  rollback_test::JitterTransport tBA({0x2222, dBA, dBA, 0.0, 0.0});

  a->setInputCallbacks(
      [&](uint8_t gen, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
        tAB.sendAToB(gen, bf, c, in, lf);
      });
  b->setInputCallbacks(
      [&](uint8_t gen, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
        tBA.sendAToB(gen, bf, c, in, lf);  // B's outbound rides the B→A pipe
      });
  a->focus();
  b->focus();

  for (uint32_t f = 0; f < 3; ++f) {
    a->injectRemoteInput(f, 0);
    b->injectRemoteInput(f, 0);
  }

  auto deliverB = [&](uint8_t /*gen*/, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
    b->injectRemoteBatch(bf, c, in, lf);
  };
  auto deliverA = [&](uint8_t /*gen*/, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
    a->injectRemoteBatch(bf, c, in, lf);
  };
  auto deliverNoop =
      [&](uint8_t, uint32_t, uint8_t, uint8_t const*, uint32_t) {};

  int maxAbsGap = 0;
  for (int i = 0; i < ticks; ++i) {
    a->setLocalControlState(0);
    b->setLocalControlState(0);
    a->process();
    b->process();
    // tAB pipes A→B inputs only; the "B leg" of its tick stays a no-op.
    tAB.tick(deliverNoop, deliverB);
    tBA.tick(deliverNoop, deliverA);

    // Window starts after both peers have had time to learn each other
    // (>= max(dAB, dBA)*4) so warm-up doesn't pollute the maximum.
    if (i > 4 * (dAB > dBA ? dAB : dBA)) {
      int gap = static_cast<int>(a->currentFrame()) -
                static_cast<int>(b->currentFrame());
      int abs = gap < 0 ? -gap : gap;
      if (abs > maxAbsGap) maxAbsGap = abs;
    }
  }

  return {maxAbsGap, a->frameAdvantageStallCount(),
          b->frameAdvantageStallCount()};
}

}  // namespace

TEST_CASE("Frame advantage caps the simFrame gap under asymmetric delay",
          "[rollback][frame-advantage]") {
  SECTION("D_AB=2, D_BA=6 — the plan's headline scenario") {
    auto r = runAsymmetric(2, 6, 1000);
    INFO("max |gap| = " << r.maxAbsGap
         << " stallsA=" << r.stallsA << " stallsB=" << r.stallsB);

    // The slower peer must have done meaningful stall work, otherwise
    // the test is vacuously passing.
    REQUIRE((r.stallsA > 0 || r.stallsB > 0));

    // The gap is bounded by a small constant (independent of run
    // length). The algorithm's steady state oscillates within
    // kFrameAdvantage of equilibrium plus a small overshoot during
    // the catch-up cycle, so the empirical bound sits comfortably
    // under 2*kFrameAdvantage.
    REQUIRE(r.maxAbsGap <= 2 * RollbackController::kFrameAdvantage);
  }

  SECTION("symmetric delay still works") {
    auto r = runAsymmetric(3, 3, 500);
    INFO("max |gap| = " << r.maxAbsGap
         << " stallsA=" << r.stallsA << " stallsB=" << r.stallsB);
    // With symmetric one-way delay the gap stays within
    // kFrameAdvantage by construction.
    REQUIRE(r.maxAbsGap <= 2 * RollbackController::kFrameAdvantage);
  }
}
