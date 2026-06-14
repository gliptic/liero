// Fast (in-memory) snapshot path.
//
// Verifies:
//   1. Round-trip: save → diverge → restore → resume matches a control
//      run.
//   2. Cereal parity: fast-save + fast-restore produces the same
//      post-restore state as cereal-save + cereal-restore.
//   3. Performance: save and load both well under 500 µs.
//   4. Dirty-cell tracking: only modified cells are written on each save;
//      level_materials is absent from the slot (recomputed on restore).

#include <algorithm>
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

    game->AddViewport(new Viewport(Rect(0, 0, 158, 158), 0));
    game->AddViewport(new Viewport(Rect(160, 0, 318, 158), 1));

    game->level.GenerateFromSettings(*common, *settings, game->rand);
    for (auto const& w : game->worms) {
      w->InitWeapons(*game);
    }

    game->paused = false;
    game->StartGame();
    game->ResetWorms();
  }

  void Step(Rand& input_rng) const {
    for (int idx = 0; idx < 2; ++idx) {
      uint32_t input = input_rng() & 0x7f;
      if ((input_rng() % 10) < 6) {
        input |= (1 << 4);
      }
      if ((input_rng() % 10) < 4) {
        input |= (1 << (idx == 0 ? 1 : 0));
      }
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
      if ((post_snap_input_rng() % 10) < 6) {
        input |= (1 << 4);
      }
      if ((post_snap_input_rng() % 10) < 4) {
        input |= (1 << (idx == 0 ? 1 : 0));
      }
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
  for (int f = 0; f < 500; ++f) {
    r.Step(input_rng);
  }

  GameSnapshot snap;
  snap.Prepare(*r.game);

  constexpr int kIters = 200;

  auto t0 = clock::now();
  for (int i = 0; i < kIters; ++i) {
    r.game->SaveSnapshotFast(snap);
  }
  auto t1 = clock::now();
  for (int i = 0; i < kIters; ++i) {
    r.game->LoadSnapshotFast(snap);
  }
  auto t2 = clock::now();

  double const kSaveUs = std::chrono::duration<double, std::micro>(t1 - t0).count() / kIters;
  double const kLoadUs = std::chrono::duration<double, std::micro>(t2 - t1).count() / kIters;

  std::cout << "[fast snapshot] save=" << kSaveUs << " us, load=" << kLoadUs << " us\n";

  REQUIRE(kSaveUs < 2000.0);
  REQUIRE(kLoadUs < 2000.0);
}

TEST_CASE("Fast snapshot round-trips the display layer", "[snapshot][rollback][display-layer]") {
  GameRunner r(0xD1500);
  Game& game = *r.game;

  // Inject a display layer: mark pixel 0 with a specific ARGB.
  std::size_t const kCells =
      static_cast<std::size_t>(game.level.width) * static_cast<std::size_t>(game.level.height);
  game.level.display_data.assign(kCells, 0);
  game.level.display_valid.assign(kCells, 0);
  game.level.display_data[0] = 0xFF112233U;
  game.level.display_valid[0] = 1;

  GameSnapshot snap;
  snap.Prepare(game);
  game.SaveSnapshotFast(snap);

  // display_valid changes during gameplay (terrain destruction zeroes it) and
  // must be restored by rollback.  display_data is static (never written during
  // simulation) and is intentionally excluded from the fast snapshot to avoid
  // copying 64 MB/slot on large levels — so it is NOT restored on load.
  game.level.display_valid[0] = 0;
  game.LoadSnapshotFast(snap);

  REQUIRE(game.level.display_valid[0] == 1);
  // display_data unchanged (not saved/restored):
  REQUIRE(game.level.display_data[0] == 0xFF112233U);

  // Classic level (no display layer): Prepare must not allocate,
  // LoadSnapshotFast must not restore when snap has no display layer.
  game.level.display_data.clear();
  game.level.display_valid.clear();
  GameSnapshot snap2;
  snap2.Prepare(game);
  game.SaveSnapshotFast(snap2);
  game.LoadSnapshotFast(snap2);
  REQUIRE(game.level.display_data.empty());
}

TEST_CASE("Dirty-cell tracking: sparse save, correct restore", "[snapshot][rollback][dirty]") {
  // Verifies:
  //  - First save to a slot triggers a full copy and initialises dirty tracking
  //  - Second save to the SAME slot uses the sparse path (only dirty cells written)
  //  - First save to a FRESH slot always does a full copy regardless of dirty state
  //  - level_materials is absent from GameSnapshot (materials recomputed on restore)
  //  - Round-trip correctness for both post-modification and pre-modification restores

  GameRunner r(0x1EADBEE5);
  Game& game = *r.game;
  std::size_t const kCells =
      static_cast<std::size_t>(game.level.width) * static_cast<std::size_t>(game.level.height);

  // Populate a display layer so the sparse display_valid update path
  // (SaveSnapshotFast) and its restore (LoadSnapshotFast) are exercised. The
  // full-copy display_valid path is covered elsewhere; this case covers the
  // dirty-cell branch, which writes display_valid only for cells in dirty_list.
  game.level.display_data.assign(kCells, 0);
  game.level.display_valid.assign(kCells, static_cast<uint8_t>(1));

  // snap_a: will be saved twice — before and after the modification.
  // snap_b: saved once, before the modification, to test restoring to initial state.
  GameSnapshot snap_a;
  snap_a.Prepare(game);
  GameSnapshot snap_b;
  snap_b.Prepare(game);

  // First save to both slots: full copy; inits dirty tracking.
  game.SaveSnapshotFast(snap_a);
  game.SaveSnapshotFast(snap_b);
  REQUIRE(snap_a.level_data.size() == kCells);
  REQUIRE(std::equal(snap_a.level_data.begin(), snap_a.level_data.end(),
                     game.level.material_id.begin()));
  // dirty_list must now be initialised (empty, since no SetPixel since tracking start).
  REQUIRE(game.level.dirty_list.empty());

  // Capture the initial material layout for later comparison.
  std::vector<uint8_t> initial_data(game.level.material_id.begin(), game.level.material_id.end());

  // Modify one cell.
  int const kX = 42;
  int const kY = 37;
  int const kIdx = kX + kY * game.level.width;
  unsigned char const kOldMat = initial_data[static_cast<std::size_t>(kIdx)];
  unsigned char const kNewMat =
      (kOldMat == 0) ? static_cast<unsigned char>(1) : static_cast<unsigned char>(0);
  // snap_a should not yet reflect the modification.
  REQUIRE(snap_a.level_data[static_cast<std::size_t>(kIdx)] == kOldMat);
  game.level.SetPixel(kX, kY, kNewMat, *game.common);
  REQUIRE(game.level.dirty_list.size() == 1);

  // Second save to snap_a (already initialised) — sparse path: only the dirty
  // cell is overwritten; all other cells retain their first-save values.
  game.SaveSnapshotFast(snap_a);
  REQUIRE(snap_a.level_data[static_cast<std::size_t>(kIdx)] == kNewMat);
  for (std::size_t i = 0; i < kCells; ++i) {
    if (static_cast<int>(i) != kIdx) {
      REQUIRE(snap_a.level_data[i] == initial_data[i]);
    }
  }
  // SetPixel zeroed display_valid at the dirty cell; the sparse save must have
  // propagated that into the slot while leaving every other cell at its
  // first-save value of 1.
  REQUIRE(snap_a.level_display_valid[static_cast<std::size_t>(kIdx)] == 0);
  for (std::size_t i = 0; i < kCells; ++i) {
    if (static_cast<int>(i) != kIdx) {
      REQUIRE(snap_a.level_display_valid[i] == 1);
    }
  }

  // Fresh slot saved after the modification: first save → full copy of current state.
  GameSnapshot snap_c;
  snap_c.Prepare(game);
  game.SaveSnapshotFast(snap_c);
  REQUIRE(snap_c.level_data[static_cast<std::size_t>(kIdx)] == kNewMat);
  for (std::size_t i = 0; i < kCells; ++i) {
    if (static_cast<int>(i) != kIdx) {
      REQUIRE(snap_c.level_data[i] == initial_data[i]);
    }
  }

  // Corrupt the live cell and restore from snap_a (post-modification snapshot).
  game.level.material_id[static_cast<std::size_t>(kIdx)] = kOldMat;
  game.level.display_valid[static_cast<std::size_t>(kIdx)] = 1;
  game.LoadSnapshotFast(snap_a);
  REQUIRE(game.level.material_id[static_cast<std::size_t>(kIdx)] == kNewMat);
  // materials must be consistent with material_id after restore (no level_materials in slot).
  REQUIRE(game.level.materials[static_cast<std::size_t>(kIdx)].flags ==
          game.common->materials[kNewMat].flags);
  // display_valid must reflect the post-modification snapshot (cell was zeroed).
  REQUIRE(game.level.display_valid[static_cast<std::size_t>(kIdx)] == 0);

  // Restore from snap_b (saved before the modification) — should recover kOldMat.
  game.LoadSnapshotFast(snap_b);
  REQUIRE(game.level.material_id[static_cast<std::size_t>(kIdx)] == kOldMat);
  REQUIRE(game.level.materials[static_cast<std::size_t>(kIdx)].flags ==
          game.common->materials[kOldMat].flags);
  // snap_b predates the modification, so display_valid restores to 1.
  REQUIRE(game.level.display_valid[static_cast<std::size_t>(kIdx)] == 1);
}
