// Full network game-loop integration tests (Layer A).
//
// Drives two RollbackControllers through the in-memory JitterTransport
// to a complete KillEmAll match (both peers reach game-over), under two
// network profiles:
//
//   1. zero-jitter / no-loss — deterministic delivery; the confirmed
//      timeline must agree on every frame and no rollback-induced
//      desync may occur.
//   2. jitter + 10% packet loss — mispredictions and rollback are
//      inevitable; the steady-state confirmation lag must stay bounded
//      by kMaxRollback, rollback must actually fire, and the confirmed
//      timeline must still agree.
//
// Convergence is asserted via the confirmed-frame checksum callbacks
// (NetRunResult::desynced / compared_frames), not by comparing the two
// peers' frozen final state — see net_game_harness.hpp for why. Game-
// over is the live game's State() == kStateGameEnded latch (plan Task 0).

#include <catch2/catch_test_macros.hpp>
#include <cstdint>

#include "net_game_harness.hpp"
#include "rand.hpp"
#include "rollback/buffer.hpp"
#include "worm.hpp"

namespace {

// Aggressive damage-forcing input: random movement/aim, a high fire
// duty cycle (pulsed, so edge-triggered weapons keep shooting and worms
// ready up), plus a bias to move toward the opponent. Mirrors
// test_full_game.cpp's CombatInput so a match terminates in a bounded
// number of frames instead of wandering.
uint8_t CombatInput(Rand& rng, int peer_idx) {
  uint8_t input = rng() & 0x7f;
  if ((rng() % 10) < 6) {
    input |= (1 << Worm::kFire);
  }
  if ((rng() % 10) < 4) {
    input |= (1 << (peer_idx == 0 ? 1 : 0));  // move toward opponent
  }
  return input;
}

constexpr uint32_t kInputSeed = 0xDEAD1234;
// Single-player KillEmAll terminates well under this with lives=1 +
// low health + aggressive combat; the cap only guards a pathological
// non-terminating run.
constexpr int kMaxFrames = 200000;

}  // namespace

TEST_CASE("Layer A: two rollback peers reach game-over with zero jitter", "[net_full_game]") {
  rollback_test::RollbackPair pair = rollback_test::MakeRollbackPair({.world_seed = 0xBEEF});
  rollback_test::JitterTransport transport({.seed = 0x1234});  // delay 0, no loss

  Rand rng_a(kInputSeed);
  Rand rng_b(kInputSeed ^ 0x55555555U);
  auto const kResult = rollback_test::RunPairToCompletion(
      pair, transport, [&](int peer, int) { return CombatInput(peer == 0 ? rng_a : rng_b, peer); },
      kMaxFrames);

  INFO("frames=" << kResult.frames_elapsed << " compared=" << kResult.compared_frames
                 << " max_lag=" << kResult.max_lag);
  REQUIRE(kResult.reached_game_over);
  REQUIRE(kResult.frames_elapsed < kMaxFrames);
  // The confirmed timeline agreed on every overlapping frame...
  REQUIRE(kResult.compared_frames > 0);  // ...and the check wasn't vacuous.
  REQUIRE_FALSE(kResult.desynced);
}

TEST_CASE("Layer A: two rollback peers reach game-over under jitter + 10% loss",
          "[net_full_game]") {
  rollback_test::RollbackPair pair = rollback_test::MakeRollbackPair({.world_seed = 0xBEEF});
  rollback_test::JitterTransport transport(
      {.seed = 0x10ADED, .min_delay_frames = 1, .max_delay_frames = 3, .loss_probability = 0.10});

  Rand rng_a(kInputSeed);
  Rand rng_b(kInputSeed ^ 0x55555555U);
  auto const kResult = rollback_test::RunPairToCompletion(
      pair, transport, [&](int peer, int) { return CombatInput(peer == 0 ? rng_a : rng_b, peer); },
      kMaxFrames);

  INFO("frames=" << kResult.frames_elapsed << " compared=" << kResult.compared_frames
                 << " max_lag=" << kResult.max_lag << " rb_a=" << kResult.rollback_count_a
                 << " rb_b=" << kResult.rollback_count_b
                 << " dropped=" << transport.packets_dropped);
  REQUIRE(kResult.reached_game_over);
  REQUIRE(kResult.frames_elapsed < kMaxFrames);
  // Loss + jitter must have actually exercised rollback...
  REQUIRE(transport.packets_dropped > 0);
  REQUIRE(kResult.rollback_count_a > 0);
  REQUIRE(kResult.rollback_count_b > 0);
  // ...confirmation lag stayed within the ring's bound...
  REQUIRE(kResult.max_lag <= static_cast<uint32_t>(rollback::kMaxRollback));
  // ...and the confirmed timeline still converged.
  REQUIRE(kResult.compared_frames > 0);
  REQUIRE_FALSE(kResult.desynced);
}
