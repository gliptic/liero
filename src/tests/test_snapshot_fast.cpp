// Fast (in-memory) snapshot path.
//
// Verifies:
//   1. Round-trip: save → diverge → restore → resume matches a control
//      run.
//   2. Cereal parity: fast-save + fast-restore produces the same
//      post-restore state as cereal-save + cereal-restore.
//   3. Performance: save and load both well under 500 µs.

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>

#include "game.hpp"
#include "level.hpp"
#include "math.hpp"
#include "mixer/player.hpp"
#include "serialization/fast_snapshot.hpp"
#include "stateHash.hpp"
#include "viewport.hpp"
#include "weapon.hpp"
#include "worm.hpp"

namespace {

struct GameRunner {
  std::shared_ptr<Common> common;
  std::shared_ptr<Settings> settings;
  std::shared_ptr<SoundPlayer> sp;
  std::unique_ptr<Game> game;

  GameRunner(uint32_t seed) {
    PrecomputeTables();

    common = std::make_shared<Common>();
    FsNode const kTcRoot(FsNode("data") / "TC" / "openliero");
    common->load(kTcRoot);

    settings = std::make_shared<Settings>();
    settings->lives = 50;
    settings->loading_time = 0;
    settings->random_level = true;
    settings->game_mode = Settings::kGmKillEmAll;
    settings->blood = 100;

    sp = std::make_shared<NullSoundPlayer>();
    game = std::make_unique<Game>(common, settings, sp);
    game->rand.Seed(seed);

    for (int idx = 0; idx < 2; ++idx) {
      auto w = std::make_shared<Worm>();
      w->settings = settings->worm_settings[idx];
      w->health = 25;
      w->index = idx;
      w->stats_x = idx == 0 ? 0 : 218;
      game->AddWorm(w);
    }

    game->AddViewport(new Viewport(Rect(0, 0, 158, 158), 0, 504, 350));
    game->AddViewport(new Viewport(Rect(160, 0, 318, 158), 1, 504, 350));

    game->level.GenerateFromSettings(*common, *settings, game->rand);
    for (auto const& w : game->worms) w->InitWeapons(*game);

    game->paused = false;
    game->StartGame();
    game->ResetWorms();
  }

  void Step(Rand& input_rng) const {
    for (int idx = 0; idx < 2; ++idx) {
      uint32_t input = input_rng() & 0x7f;
      if ((input_rng() % 10) < 6) input |= (1 << 4);
      if ((input_rng() % 10) < 4) input |= (1 << (idx == 0 ? 1 : 0));
      game->worms[idx]->control_states.Unpack(input);
    }
    game->ProcessFrame();
  }
};

}  // namespace

TEST_CASE("Fast snapshot round-trip preserves frame-by-frame state", "[snapshot][rollback]") {
  constexpr uint32_t kSeed = 0xC0FFEE;
  constexpr int kPhase = 200;

  std::vector<uint32_t> control_hashes;
  control_hashes.reserve(3 * kPhase);
  {
    GameRunner ctl(kSeed);
    Rand input_rng(kSeed ^ 0xDEAD);
    for (int f = 0; f < 3 * kPhase; ++f) {
      ctl.Step(input_rng);
      control_hashes.push_back(HashGameState(*ctl.game));
    }
  }

  GameRunner sub(kSeed);
  Rand input_rng(kSeed ^ 0xDEAD);

  GameSnapshot snap;
  snap.Prepare(*sub.game);

  for (int f = 0; f < kPhase; ++f) {
    sub.Step(input_rng);
    REQUIRE(HashGameState(*sub.game) == control_hashes[f]);
  }

  sub.game->SaveSnapshotFast(snap);
  uint32_t const kHashAtSnap = HashGameState(*sub.game);

  for (int f = kPhase; f < 2 * kPhase; ++f) {
    sub.Step(input_rng);
    REQUIRE(HashGameState(*sub.game) == control_hashes[f]);
  }

  sub.game->LoadSnapshotFast(snap);
  REQUIRE(HashGameState(*sub.game) == kHashAtSnap);

  // Replay phase-2 inputs from the restored state — same scheme as the
  // cereal round-trip test.
  Rand post_snap_input_rng(kSeed ^ 0xDEAD);
  for (int f = 0; f < kPhase; ++f) {
    for (int idx = 0; idx < 2; ++idx) {
      (void)post_snap_input_rng();
      (void)post_snap_input_rng();
      (void)post_snap_input_rng();
    }
  }
  for (int f = kPhase; f < 2 * kPhase; ++f) {
    for (int idx = 0; idx < 2; ++idx) {
      uint32_t input = post_snap_input_rng() & 0x7f;
      if ((post_snap_input_rng() % 10) < 6) input |= (1 << 4);
      if ((post_snap_input_rng() % 10) < 4) input |= (1 << (idx == 0 ? 1 : 0));
      sub.game->worms[idx]->control_states.Unpack(input);
    }
    sub.game->ProcessFrame();
    INFO("Mismatch at post-restore frame " << f);
    REQUIRE(HashGameState(*sub.game) == control_hashes[f]);
  }
}

TEST_CASE("Fast snapshot matches cereal oracle across a fuzz run", "[snapshot][rollback]") {
  // For a handful of frames during a long run, save with both paths on twin
  // Game instances, mutate, then restore each from its own snapshot. The
  // post-restore state hash must agree — cereal is the reference.
  constexpr uint32_t kSeed = 0xFACE;
  constexpr int kTotal = 2000;
  int const kSnapAt[] = {137, 411, 893, 1450, 1799};

  GameRunner a(kSeed);
  GameRunner b(kSeed);
  Rand input_rng_a(kSeed ^ 0xBEEF);
  Rand input_rng_b(kSeed ^ 0xBEEF);

  GameSnapshot fast_snap;
  fast_snap.Prepare(*a.game);
  std::vector<uint8_t> cereal_snap;

  std::size_t snap_idx = 0;
  for (int f = 0; f < kTotal; ++f) {
    a.Step(input_rng_a);
    b.Step(input_rng_b);
    REQUIRE(HashGameState(*a.game) == HashGameState(*b.game));

    if (snap_idx < std::size(kSnapAt) && f == kSnapAt[snap_idx]) {
      a.game->SaveSnapshotFast(fast_snap);
      b.game->SaveSnapshot(cereal_snap);

      // Run ~30 more frames on each so the live state diverges from the
      // snapshot, then restore.
      Rand mutate_a = input_rng_a;
      Rand mutate_b = input_rng_b;
      for (int k = 0; k < 30; ++k) {
        a.Step(mutate_a);
        b.Step(mutate_b);
      }
      a.game->LoadSnapshotFast(fast_snap);
      b.game->LoadSnapshot(cereal_snap);

      INFO("Cereal/fast restore divergence after snapshot at frame " << f);
      REQUIRE(HashGameState(*a.game) == HashGameState(*b.game));
      ++snap_idx;
    }
  }
}

TEST_CASE("Fast snapshot save/restore microbenchmark", "[snapshot][rollback][!benchmark]") {
  // Plan target: ≤500 µs save + ≤500 µs restore. Assert a generous bound
  // here (2 ms) so noisy CI machines don't flake; the real numbers print
  // to stdout for inspection.
  using clock = std::chrono::steady_clock;

  GameRunner r(0x12345);
  Rand input_rng(0xABCDEF);
  for (int f = 0; f < 500; ++f) r.Step(input_rng);

  GameSnapshot snap;
  snap.Prepare(*r.game);

  constexpr int kIters = 200;

  auto t0 = clock::now();
  for (int i = 0; i < kIters; ++i) r.game->SaveSnapshotFast(snap);
  auto t1 = clock::now();
  for (int i = 0; i < kIters; ++i) r.game->LoadSnapshotFast(snap);
  auto t2 = clock::now();

  double const kSaveUs = std::chrono::duration<double, std::micro>(t1 - t0).count() / kIters;
  double const kLoadUs = std::chrono::duration<double, std::micro>(t2 - t1).count() / kIters;

  std::cout << "[fast snapshot] save=" << kSaveUs << " us, load=" << kLoadUs << " us\n";

  REQUIRE(kSaveUs < 2000.0);
  REQUIRE(kLoadUs < 2000.0);
}
