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

#include <algorithm>
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

// Run one peer-pair with the given fixed one-way delays (D_AB, D_BA)
// and return the maximum |simFrame_A - simFrame_B| observed during the
// post-warm-up window. Also surfaces stall counts so the assertion can
// guard against a vacuously-passing test (zero stalls would mean the
// delays were symmetric enough that the threshold never fired).
struct AsymmetricRun {
  int max_abs_gap;
  uint64_t stalls_a;
  uint64_t stalls_b;
};

AsymmetricRun RunAsymmetric(int d_ab, int d_ba, int ticks) {
  auto [common, settings] = MakeEnv();
  auto a = std::make_unique<RollbackController>(common, settings, 0);
  auto b = std::make_unique<RollbackController>(common, settings, 1);
  a->SetSkipWeaponSelection(/*skip=*/true);
  b->SetSkipWeaponSelection(/*skip=*/true);
  a->game.rand.Seed(0xBEEF);
  b->game.rand.Seed(0xBEEF);

  // Asymmetric one-way delays modelled by two separate "transports":
  // one for A→B (fixed delay dAB), one for B→A (fixed delay dBA).
  // Loss/dup/jitter are off so the test isolates the time-sync feedback.
  rollback_test::JitterTransport t_ab({.seed = 0x1111,
                                       .min_delay_frames = d_ab,
                                       .max_delay_frames = d_ab,
                                       .loss_probability = 0.0,
                                       .duplicate_probability = 0.0});
  rollback_test::JitterTransport t_ba({.seed = 0x2222,
                                       .min_delay_frames = d_ba,
                                       .max_delay_frames = d_ba,
                                       .loss_probability = 0.0,
                                       .duplicate_probability = 0.0});

  a->SetInputCallbacks([&](uint8_t gen, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
    t_ab.SendAToB(gen, bf, c, in, lf);
  });
  b->SetInputCallbacks([&](uint8_t gen, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
    t_ba.SendAToB(gen, bf, c, in, lf);  // B's outbound rides the B→A pipe
  });
  a->Focus();
  b->Focus();

  for (uint32_t f = 0; f < 3; ++f) {
    a->InjectRemoteInput(f, 0);
    b->InjectRemoteInput(f, 0);
  }

  auto deliver_b = [&](uint8_t /*gen*/, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
    b->InjectRemoteBatch(bf, c, in, lf);
  };
  auto deliver_a = [&](uint8_t /*gen*/, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
    a->InjectRemoteBatch(bf, c, in, lf);
  };
  auto deliver_noop = [&](uint8_t, uint32_t, uint8_t, uint8_t const*, uint32_t) {};

  int max_abs_gap = 0;
  for (int i = 0; i < ticks; ++i) {
    a->SetLocalControlState(0);
    b->SetLocalControlState(0);
    a->Process();
    b->Process();
    // tAB pipes A→B inputs only; the "B leg" of its tick stays a no-op.
    t_ab.Tick(deliver_noop, deliver_b);
    t_ba.Tick(deliver_noop, deliver_a);

    // Window starts after both peers have had time to learn each other
    // (>= max(dAB, dBA)*4) so warm-up doesn't pollute the maximum.
    if (i > 4 * (d_ab > d_ba ? d_ab : d_ba)) {
      int const kGap = static_cast<int>(a->CurrentFrame()) - static_cast<int>(b->CurrentFrame());
      int const kAbs = kGap < 0 ? -kGap : kGap;
      max_abs_gap = std::max(kAbs, max_abs_gap);
    }
  }

  return {.max_abs_gap = max_abs_gap,
          .stalls_a = a->FrameAdvantageStallCount(),
          .stalls_b = b->FrameAdvantageStallCount()};
}

}  // namespace

TEST_CASE("Frame advantage caps the simFrame gap under asymmetric delay",
          "[rollback][frame-advantage]") {
  SECTION("D_AB=2, D_BA=6 — the plan's headline scenario") {
    auto r = RunAsymmetric(2, 6, 1000);
    INFO("max |gap| = " << r.max_abs_gap << " stallsA=" << r.stalls_a << " stallsB=" << r.stalls_b);

    // The slower peer must have done meaningful stall work, otherwise
    // the test is vacuously passing.
    REQUIRE((r.stalls_a > 0 || r.stalls_b > 0));

    // The gap is bounded by a small constant (independent of run
    // length). The algorithm's steady state oscillates within
    // kFrameAdvantage of equilibrium plus a small overshoot during
    // the catch-up cycle, so the empirical bound sits comfortably
    // under 2*kFrameAdvantage.
    REQUIRE(r.max_abs_gap <= 2 * RollbackController::kFrameAdvantage);
  }

  SECTION("symmetric delay still works") {
    auto r = RunAsymmetric(3, 3, 500);
    INFO("max |gap| = " << r.max_abs_gap << " stallsA=" << r.stalls_a << " stallsB=" << r.stalls_b);
    // With symmetric one-way delay the gap stays within
    // kFrameAdvantage by construction.
    REQUIRE(r.max_abs_gap <= 2 * RollbackController::kFrameAdvantage);
  }
}
