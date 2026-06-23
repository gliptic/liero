#include <catch2/catch_test_macros.hpp>
#include <cstdint>

#include "game_harness.hpp"
#include "rand.hpp"
#include "stateHash.hpp"

namespace {

uint8_t CombatInput(Rand& rng, int idx) {
  uint8_t input = rng() & 0x7f;
  if ((rng() % 10) < 6) {
    input |= (1 << 4);  // fire
  }
  if ((rng() % 10) < 4) {
    input |= (1 << (idx == 0 ? 1 : 0));  // move toward opponent
  }
  return input;
}

constexpr HeadlessGameConfig kKillEmAllCfg{.seed = 42, .lives = 2, .health = 25};
constexpr uint32_t kInputSeed = 0xDEAD1234;
constexpr int kMaxFrames = 300000;

}  // namespace

TEST_CASE("KillEmAll game reaches game over via scripted input", "[full_game]") {
  auto game = MakeHeadlessGame(kKillEmAllCfg);
  Rand input_rng(kInputSeed);

  auto const kResult = RunToCompletion(
      *game, [&input_rng](int idx, int) { return CombatInput(input_rng, idx); }, kMaxFrames);

  INFO("Frames elapsed: " << kResult.frames_elapsed);
  REQUIRE(kResult.reached_game_over);
  REQUIRE(kResult.frames_elapsed < kMaxFrames);
}

TEST_CASE("KillEmAll end-state satisfies termination invariants", "[full_game]") {
  auto game = MakeHeadlessGame(kKillEmAllCfg);
  Rand input_rng(kInputSeed);

  RunToCompletion(*game, [&input_rng](int idx, int) { return CombatInput(input_rng, idx); });

  REQUIRE(game->cycles > 0);

  bool any_lives_depleted = false;
  for (auto const& w : game->worms) {
    if (w->lives <= 0) {
      any_lives_depleted = true;
    }
    REQUIRE(w->health <= w->settings->health);
  }
  REQUIRE(any_lives_depleted);
}

TEST_CASE("Full game run is deterministic", "[full_game][determinism]") {
  uint32_t hash1 = 0;
  uint32_t hash2 = 0;

  for (int run = 0; run < 2; ++run) {
    auto game = MakeHeadlessGame(kKillEmAllCfg);
    Rand input_rng(kInputSeed);

    RunToCompletion(*game, [&input_rng](int idx, int) { return CombatInput(input_rng, idx); });

    uint32_t const kHash = HashGameState(*game);
    if (run == 0) {
      hash1 = kHash;
    } else {
      hash2 = kHash;
    }
  }

  REQUIRE(hash1 == hash2);
}

TEST_CASE("KillEmAll terminates across a sweep of seeds", "[full_game][sweep]") {
  for (uint32_t seed = 0; seed < 100; ++seed) {
    INFO("seed=" << seed);
    auto game = MakeHeadlessGame({.seed = seed, .lives = 2, .health = 25});
    Rand input_rng(kInputSeed);
    auto const kResult = RunToCompletion(
        *game, [&input_rng](int idx, int) { return CombatInput(input_rng, idx); }, kMaxFrames);
    REQUIRE(kResult.reached_game_over);
    REQUIRE(kResult.frames_elapsed < kMaxFrames);
  }
}

TEST_CASE("GameOfTag terminates when it-timer reaches time_to_lose", "[full_game][smoke]") {
  // Timer increments once per 70 frames while the last killer is visible;
  // time_to_lose=1 means a single 70-frame tick after the first kill suffices.
  auto game = MakeHeadlessGame({.seed = 42, .game_mode = Settings::kGmGameOfTag});
  game->settings->time_to_lose = 1;

  Rand input_rng(kInputSeed);
  auto const kResult = RunToCompletion(
      *game, [&input_rng](int idx, int) { return CombatInput(input_rng, idx); }, 5000);

  INFO("Frames elapsed: " << kResult.frames_elapsed);
  REQUIRE(kResult.reached_game_over);
}

TEST_CASE("Holdazone terminates when zone-timer reaches time_to_lose", "[full_game][smoke]") {
  // Timer increments once per 70 frames while holdazone.holder_idx is set.
  // Pre-set worm 0 as holder so the test doesn't depend on random zone capture.
  auto game = MakeHeadlessGame({.seed = 42, .game_mode = Settings::kGmHoldazone});
  game->settings->time_to_lose = 1;
  game->holdazone.holder_idx = 0;
  game->holdazone.timeout_left = 1000;

  auto const kResult = RunToCompletion(*game, [](int, int) { return uint8_t{0}; }, 200);

  INFO("Frames elapsed: " << kResult.frames_elapsed);
  REQUIRE(kResult.reached_game_over);
}
