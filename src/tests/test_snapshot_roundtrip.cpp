// Cereal-based snapshot round-trip — correctness oracle for the fast
// in-memory snapshot path.
//
// Run N frames, save snapshot, run another N frames, restore, run
// another N frames; assert the per-frame state hash matches a control
// simulation that never restored.

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>

#include "game.hpp"
#include "level.hpp"
#include "math.hpp"
#include "mixer/player.hpp"
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
      if ((input_rng() % 10) < 6) input |= (1 << 4);  // fire
      if ((input_rng() % 10) < 4) input |= (1 << (idx == 0 ? 1 : 0));
      game->worms[idx]->control_states.Unpack(input);
    }
    game->ProcessFrame();
  }
};

}  // namespace

TEST_CASE("Snapshot round-trip preserves frame-by-frame state", "[snapshot][rollback]") {
  constexpr uint32_t kSeed = 0xC0FFEE;
  constexpr int kPhase = 200;

  // Control: run 3*kPhase frames straight, recording per-frame hashes.
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

  // Subject: same seed and inputs, but with a snapshot/restore in the middle.
  GameRunner sub(kSeed);
  Rand input_rng(kSeed ^ 0xDEAD);

  // Phase 1: frames [0, kPhase) — verify match against control.
  for (int f = 0; f < kPhase; ++f) {
    sub.Step(input_rng);
    REQUIRE(HashGameState(*sub.game) == control_hashes[f]);
  }

  // Snapshot at frame kPhase.
  std::vector<uint8_t> snap;
  sub.game->SaveSnapshot(snap);
  uint32_t const kHashAtSnap = HashGameState(*sub.game);

  // Phase 2: frames [kPhase, 2*kPhase) — advance, ignore final hash.
  for (int f = kPhase; f < 2 * kPhase; ++f) {
    sub.Step(input_rng);
    REQUIRE(HashGameState(*sub.game) == control_hashes[f]);
  }

  // Restore the snapshot — state must match exactly what we saved.
  sub.game->LoadSnapshot(snap);
  REQUIRE(HashGameState(*sub.game) == kHashAtSnap);

  // Phase 3: replay phase-2 inputs from the restored state. Rebuild an
  // input PRNG and skip phase-1's draws so it lines up with the control
  // at frame kPhase.
  Rand post_snap_input_rng(kSeed ^ 0xDEAD);
  for (int f = 0; f < kPhase; ++f) {
    for (int idx = 0; idx < 2; ++idx) {
      (void)post_snap_input_rng();  // input bits
      (void)post_snap_input_rng();  // fire roll
      (void)post_snap_input_rng();  // movement roll
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

TEST_CASE("Snapshot save/restore microbenchmark", "[snapshot][rollback][!benchmark]") {
  // Informational; asserts a generous upper bound to catch regressions.
  // Real cereal cost on commodity hw is ~1-3 ms; the fast path is
  // ~100x faster.

  using clock = std::chrono::steady_clock;

  GameRunner r(0x12345);
  Rand input_rng(0xABCDEF);
  for (int f = 0; f < 500; ++f) r.Step(input_rng);

  constexpr int kIters = 50;
  std::vector<uint8_t> snap;

  auto t0 = clock::now();
  for (int i = 0; i < kIters; ++i) {
    snap.clear();
    r.game->SaveSnapshot(snap);
  }
  auto t1 = clock::now();
  for (int i = 0; i < kIters; ++i) {
    r.game->LoadSnapshot(snap);
  }
  auto t2 = clock::now();

  double const kSaveUs = std::chrono::duration<double, std::micro>(t1 - t0).count() / kIters;
  double const kLoadUs = std::chrono::duration<double, std::micro>(t2 - t1).count() / kIters;

  std::cout << "[snapshot bench] save=" << kSaveUs << " us, load=" << kLoadUs
            << " us, size=" << snap.size() << " bytes\n";

  // Generous bound: 10 ms.
  REQUIRE(kSaveUs < 10000.0);
  REQUIRE(kLoadUs < 10000.0);
}
