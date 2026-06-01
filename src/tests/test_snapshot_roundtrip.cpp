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
    precomputeTables();

    common = std::make_shared<Common>();
    FsNode tcRoot(FsNode("data") / "TC" / "openliero");
    common->load(std::move(tcRoot));

    settings = std::make_shared<Settings>();
    settings->lives = 50;
    settings->loadingTime = 0;
    settings->randomLevel = true;
    settings->gameMode = Settings::GMKillEmAll;
    settings->blood = 100;

    sp = std::make_shared<NullSoundPlayer>();
    game = std::make_unique<Game>(common, settings, sp);
    game->rand.seed(seed);

    for (int idx = 0; idx < 2; ++idx) {
      auto w = std::make_shared<Worm>();
      w->settings = settings->wormSettings[idx];
      w->health = 25;
      w->index = idx;
      w->statsX = idx == 0 ? 0 : 218;
      game->addWorm(w);
    }

    game->addViewport(new Viewport(Rect(0, 0, 158, 158), 0, 504, 350));
    game->addViewport(new Viewport(Rect(160, 0, 318, 158), 1, 504, 350));

    game->level.generateFromSettings(*common, *settings, game->rand);
    for (auto const& w : game->worms) w->initWeapons(*game);

    game->paused = false;
    game->startGame();
    game->resetWorms();
  }

  void step(Rand& inputRng) {
    for (int idx = 0; idx < 2; ++idx) {
      uint32_t input = inputRng() & 0x7f;
      if ((inputRng() % 10) < 6) input |= (1 << 4);  // fire
      if ((inputRng() % 10) < 4) input |= (1 << (idx == 0 ? 1 : 0));
      game->worms[idx]->controlStates.unpack(input);
    }
    game->processFrame();
  }
};

}  // namespace

TEST_CASE("Snapshot round-trip preserves frame-by-frame state", "[snapshot][rollback]") {
  constexpr uint32_t kSeed = 0xC0FFEE;
  constexpr int kPhase = 200;

  // Control: run 3*kPhase frames straight, recording per-frame hashes.
  std::vector<uint32_t> controlHashes;
  controlHashes.reserve(3 * kPhase);
  {
    GameRunner ctl(kSeed);
    Rand inputRng(kSeed ^ 0xDEAD);
    for (int f = 0; f < 3 * kPhase; ++f) {
      ctl.step(inputRng);
      controlHashes.push_back(hashGameState(*ctl.game));
    }
  }

  // Subject: same seed and inputs, but with a snapshot/restore in the middle.
  GameRunner sub(kSeed);
  Rand inputRng(kSeed ^ 0xDEAD);

  // Phase 1: frames [0, kPhase) — verify match against control.
  for (int f = 0; f < kPhase; ++f) {
    sub.step(inputRng);
    REQUIRE(hashGameState(*sub.game) == controlHashes[f]);
  }

  // Snapshot at frame kPhase.
  std::vector<uint8_t> snap;
  sub.game->saveSnapshot(snap);
  uint32_t hashAtSnap = hashGameState(*sub.game);

  // Phase 2: frames [kPhase, 2*kPhase) — advance, ignore final hash.
  for (int f = kPhase; f < 2 * kPhase; ++f) {
    sub.step(inputRng);
    REQUIRE(hashGameState(*sub.game) == controlHashes[f]);
  }

  // Restore the snapshot — state must match exactly what we saved.
  sub.game->loadSnapshot(snap);
  REQUIRE(hashGameState(*sub.game) == hashAtSnap);

  // Phase 3: replay phase-2 inputs from the restored state. Rebuild an
  // input PRNG and skip phase-1's draws so it lines up with the control
  // at frame kPhase.
  Rand postSnapInputRng(kSeed ^ 0xDEAD);
  for (int f = 0; f < kPhase; ++f) {
    for (int idx = 0; idx < 2; ++idx) {
      (void)postSnapInputRng();  // input bits
      (void)postSnapInputRng();  // fire roll
      (void)postSnapInputRng();  // movement roll
    }
  }
  for (int f = kPhase; f < 2 * kPhase; ++f) {
    for (int idx = 0; idx < 2; ++idx) {
      uint32_t input = postSnapInputRng() & 0x7f;
      if ((postSnapInputRng() % 10) < 6) input |= (1 << 4);
      if ((postSnapInputRng() % 10) < 4) input |= (1 << (idx == 0 ? 1 : 0));
      sub.game->worms[idx]->controlStates.unpack(input);
    }
    sub.game->processFrame();
    INFO("Mismatch at post-restore frame " << f);
    REQUIRE(hashGameState(*sub.game) == controlHashes[f]);
  }
}

TEST_CASE("Snapshot save/restore microbenchmark", "[snapshot][rollback][!benchmark]") {
  // Informational; asserts a generous upper bound to catch regressions.
  // Real cereal cost on commodity hw is ~1-3 ms; the fast path is
  // ~100x faster.

  using clock = std::chrono::steady_clock;

  GameRunner r(0x12345);
  Rand inputRng(0xABCDEF);
  for (int f = 0; f < 500; ++f) r.step(inputRng);

  constexpr int kIters = 50;
  std::vector<uint8_t> snap;

  auto t0 = clock::now();
  for (int i = 0; i < kIters; ++i) {
    snap.clear();
    r.game->saveSnapshot(snap);
  }
  auto t1 = clock::now();
  for (int i = 0; i < kIters; ++i) {
    r.game->loadSnapshot(snap);
  }
  auto t2 = clock::now();

  double saveUs = std::chrono::duration<double, std::micro>(t1 - t0).count() / kIters;
  double loadUs = std::chrono::duration<double, std::micro>(t2 - t1).count() / kIters;

  std::cout << "[snapshot bench] save=" << saveUs << " us, load=" << loadUs
            << " us, size=" << snap.size() << " bytes\n";

  // Generous bound: 10 ms.
  REQUIRE(saveUs < 10000.0);
  REQUIRE(loadUs < 10000.0);
}
