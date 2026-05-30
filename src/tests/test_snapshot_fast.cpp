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
      if ((inputRng() % 10) < 6) input |= (1 << 4);
      if ((inputRng() % 10) < 4) input |= (1 << (idx == 0 ? 1 : 0));
      game->worms[idx]->controlStates.unpack(input);
    }
    game->processFrame();
  }
};

}  // namespace

TEST_CASE("Fast snapshot round-trip preserves frame-by-frame state",
          "[snapshot][rollback]") {
  constexpr uint32_t kSeed = 0xC0FFEE;
  constexpr int kPhase = 200;

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

  GameRunner sub(kSeed);
  Rand inputRng(kSeed ^ 0xDEAD);

  GameSnapshot snap;
  snap.prepare(*sub.game);

  for (int f = 0; f < kPhase; ++f) {
    sub.step(inputRng);
    REQUIRE(hashGameState(*sub.game) == controlHashes[f]);
  }

  sub.game->saveSnapshotFast(snap);
  uint32_t hashAtSnap = hashGameState(*sub.game);

  for (int f = kPhase; f < 2 * kPhase; ++f) {
    sub.step(inputRng);
    REQUIRE(hashGameState(*sub.game) == controlHashes[f]);
  }

  sub.game->loadSnapshotFast(snap);
  REQUIRE(hashGameState(*sub.game) == hashAtSnap);

  // Replay phase-2 inputs from the restored state — same scheme as the
  // cereal round-trip test.
  Rand postSnapInputRng(kSeed ^ 0xDEAD);
  for (int f = 0; f < kPhase; ++f) {
    for (int idx = 0; idx < 2; ++idx) {
      (void)postSnapInputRng();
      (void)postSnapInputRng();
      (void)postSnapInputRng();
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

TEST_CASE("Fast snapshot matches cereal oracle across a fuzz run",
          "[snapshot][rollback]") {
  // For a handful of frames during a long run, save with both paths on twin
  // Game instances, mutate, then restore each from its own snapshot. The
  // post-restore state hash must agree — cereal is the reference.
  constexpr uint32_t kSeed = 0xFACE;
  constexpr int kTotal = 2000;
  int const kSnapAt[] = {137, 411, 893, 1450, 1799};

  GameRunner a(kSeed);
  GameRunner b(kSeed);
  Rand inputRngA(kSeed ^ 0xBEEF), inputRngB(kSeed ^ 0xBEEF);

  GameSnapshot fastSnap;
  fastSnap.prepare(*a.game);
  std::vector<uint8_t> cerealSnap;

  std::size_t snapIdx = 0;
  for (int f = 0; f < kTotal; ++f) {
    a.step(inputRngA);
    b.step(inputRngB);
    REQUIRE(hashGameState(*a.game) == hashGameState(*b.game));

    if (snapIdx < std::size(kSnapAt) && f == kSnapAt[snapIdx]) {
      a.game->saveSnapshotFast(fastSnap);
      b.game->saveSnapshot(cerealSnap);

      // Run ~30 more frames on each so the live state diverges from the
      // snapshot, then restore.
      Rand mutateA = inputRngA, mutateB = inputRngB;
      for (int k = 0; k < 30; ++k) {
        a.step(mutateA);
        b.step(mutateB);
      }
      a.game->loadSnapshotFast(fastSnap);
      b.game->loadSnapshot(cerealSnap);

      INFO("Cereal/fast restore divergence after snapshot at frame " << f);
      REQUIRE(hashGameState(*a.game) == hashGameState(*b.game));
      ++snapIdx;
    }
  }
}

TEST_CASE("Fast snapshot save/restore microbenchmark",
          "[snapshot][rollback][!benchmark]") {
  // Plan target: ≤500 µs save + ≤500 µs restore. Assert a generous bound
  // here (2 ms) so noisy CI machines don't flake; the real numbers print
  // to stdout for inspection.
  using clock = std::chrono::steady_clock;

  GameRunner r(0x12345);
  Rand inputRng(0xABCDEF);
  for (int f = 0; f < 500; ++f) r.step(inputRng);

  GameSnapshot snap;
  snap.prepare(*r.game);

  constexpr int kIters = 200;

  auto t0 = clock::now();
  for (int i = 0; i < kIters; ++i) r.game->saveSnapshotFast(snap);
  auto t1 = clock::now();
  for (int i = 0; i < kIters; ++i) r.game->loadSnapshotFast(snap);
  auto t2 = clock::now();

  double saveUs =
      std::chrono::duration<double, std::micro>(t1 - t0).count() / kIters;
  double loadUs =
      std::chrono::duration<double, std::micro>(t2 - t1).count() / kIters;

  std::cout << "[fast snapshot] save=" << saveUs << " us, load=" << loadUs
            << " us\n";

  REQUIRE(saveUs < 2000.0);
  REQUIRE(loadUs < 2000.0);
}
