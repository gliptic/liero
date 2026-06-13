#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <memory>

#include "game.hpp"
#include "level.hpp"
#include "math.hpp"
#include "mixer/player.hpp"
#include "stateHash.hpp"
#include "viewport.hpp"
#include "weapon.hpp"
#include "worm.hpp"

struct DualGameFixture {
  std::shared_ptr<Common> common;
  std::shared_ptr<Settings> settings;
  std::shared_ptr<SoundPlayer> sound_player_a;
  std::shared_ptr<SoundPlayer> sound_player_b;
  std::unique_ptr<Game> game_a;
  std::unique_ptr<Game> game_b;

  DualGameFixture() {
    PrecomputeTables();

    common = std::make_shared<Common>();
    FsNode const kTcRoot(FsNode("data") / "TC" / "openliero");
    common->load(kTcRoot);

    settings = std::make_shared<Settings>();
    // Use default settings but ensure deterministic setup
    settings->lives = 10;
    settings->loading_time = 0;
    settings->random_level = true;
    settings->game_mode = Settings::kGmKillEmAll;

    sound_player_a = std::make_shared<NullSoundPlayer>();
    sound_player_b = std::make_shared<NullSoundPlayer>();

    game_a = std::make_unique<Game>(common, settings, sound_player_a);
    game_b = std::make_unique<Game>(common, settings, sound_player_b);

    // Seed both with the same value
    uint32_t const kSeed = 42;
    game_a->rand.Seed(kSeed);
    game_b->rand.Seed(kSeed);

    // Create identical worms for both games
    for (int idx = 0; idx < 2; ++idx) {
      auto w_a = std::make_shared<Worm>();
      w_a->settings = settings->worm_settings[idx];
      w_a->health = w_a->settings->health;
      w_a->index = idx;
      w_a->stats_x = idx == 0 ? 0 : 218;

      auto w_b = std::make_shared<Worm>();
      w_b->settings = settings->worm_settings[idx];
      w_b->health = w_b->settings->health;
      w_b->index = idx;
      w_b->stats_x = idx == 0 ? 0 : 218;

      game_a->AddWorm(w_a);
      game_b->AddWorm(w_b);
    }

    // Add viewports (needed for processFrame's viewport logic)
    game_a->AddViewport(new Viewport(Rect(0, 0, 158, 158), 0, 504, 350));
    game_a->AddViewport(new Viewport(Rect(160, 0, 318, 158), 1, 504, 350));
    game_b->AddViewport(new Viewport(Rect(0, 0, 158, 158), 0, 504, 350));
    game_b->AddViewport(new Viewport(Rect(160, 0, 318, 158), 1, 504, 350));

    // Generate levels with the same RNG state
    game_a->level.GenerateFromSettings(*common, *settings, game_a->rand);
    game_b->level.GenerateFromSettings(*common, *settings, game_b->rand);

    // Initialize weapons for all worms
    for (auto const& w : game_a->worms) {
      w->InitWeapons(*game_a);
    }
    for (auto const& w : game_b->worms) {
      w->InitWeapons(*game_b);
    }

    // Start games
    game_a->paused = false;
    game_b->paused = false;
    game_a->StartGame();
    game_b->StartGame();

    // Reset worms into game
    game_a->ResetWorms();
    game_b->ResetWorms();
  }

  // Apply identical random inputs to both games using a separate PRNG
  void ApplyRandomInputs(Rand& input_rng) const {
    for (int idx = 0; idx < 2; ++idx) {
      uint32_t const kInput = input_rng() & 0x7f;  // 7 control bits
      game_a->worms[idx]->control_states.Unpack(kInput);
      game_b->worms[idx]->control_states.Unpack(kInput);
    }
  }
};

TEST_CASE("Dual simulation produces identical state", "[determinism]") {
  DualGameFixture f;

  Rand input_rng(12345);

  constexpr int kNumFrames = 1000;

  for (int frame = 0; frame < kNumFrames; ++frame) {
    f.ApplyRandomInputs(input_rng);
    f.game_a->ProcessFrame();
    f.game_b->ProcessFrame();

    uint32_t const kHashA = HashGameState(*f.game_a);
    uint32_t const kHashB = HashGameState(*f.game_b);

    INFO("Desync at frame " << frame << ": hashA=0x" << std::hex << kHashA << " hashB=0x"
                            << kHashB);
    REQUIRE(kHashA == kHashB);
  }
}

TEST_CASE("Simulation is reproducible across runs", "[determinism]") {
  // Run the same simulation twice from scratch and verify identical results
  uint32_t hash1 = 0;
  uint32_t hash2 = 0;

  for (int run = 0; run < 2; ++run) {
    DualGameFixture f;
    Rand input_rng(99999);

    constexpr int kNumFrames = 500;

    for (int frame = 0; frame < kNumFrames; ++frame) {
      f.ApplyRandomInputs(input_rng);
      f.game_a->ProcessFrame();
    }

    uint32_t const kH = HashGameState(*f.game_a);
    if (run == 0) {
      hash1 = kH;
    } else {
      hash2 = kH;
    }
  }

  REQUIRE(hash1 == hash2);
}

TEST_CASE("Same inputs produce same state regardless of construction order", "[determinism]") {
  // Construct games in different order but with same seed — should be identical
  auto common = std::make_shared<Common>();
  FsNode const kTcRoot(FsNode("data") / "TC" / "openliero");
  common->load(kTcRoot);

  PrecomputeTables();

  auto settings = std::make_shared<Settings>();
  settings->random_level = true;

  auto sp = std::make_shared<NullSoundPlayer>();

  // Game constructed first
  Game game1(common, settings, sp);
  game1.rand.Seed(777);

  // Game constructed after some unrelated work
  volatile int dummy = 0;
  for (int i = 0; i < 1000; ++i) {
    dummy += i;
  }
  (void)dummy;

  Game game2(common, settings, std::make_shared<NullSoundPlayer>());
  game2.rand.Seed(777);

  // Same worm setup
  for (int idx = 0; idx < 2; ++idx) {
    auto w1 = std::make_shared<Worm>();
    w1->settings = settings->worm_settings[idx];
    w1->health = w1->settings->health;
    w1->index = idx;
    game1.AddWorm(w1);

    auto w2 = std::make_shared<Worm>();
    w2->settings = settings->worm_settings[idx];
    w2->health = w2->settings->health;
    w2->index = idx;
    game2.AddWorm(w2);
  }

  game1.AddViewport(new Viewport(Rect(0, 0, 158, 158), 0, 504, 350));
  game1.AddViewport(new Viewport(Rect(160, 0, 318, 158), 1, 504, 350));
  game2.AddViewport(new Viewport(Rect(0, 0, 158, 158), 0, 504, 350));
  game2.AddViewport(new Viewport(Rect(160, 0, 318, 158), 1, 504, 350));

  game1.level.GenerateFromSettings(*common, *settings, game1.rand);
  game2.level.GenerateFromSettings(*common, *settings, game2.rand);

  for (auto const& w : game1.worms) {
    w->InitWeapons(game1);
  }
  for (auto const& w : game2.worms) {
    w->InitWeapons(game2);
  }

  game1.paused = false;
  game2.paused = false;
  game1.StartGame();
  game2.StartGame();
  game1.ResetWorms();
  game2.ResetWorms();

  Rand input_rng1(55555);
  Rand input_rng2(55555);

  for (int frame = 0; frame < 300; ++frame) {
    for (int idx = 0; idx < 2; ++idx) {
      uint32_t const kInput1 = input_rng1() & 0x7f;
      uint32_t const kInput2 = input_rng2() & 0x7f;
      game1.worms[idx]->control_states.Unpack(kInput1);
      game2.worms[idx]->control_states.Unpack(kInput2);
    }
    game1.ProcessFrame();
    game2.ProcessFrame();
  }

  REQUIRE(HashGameState(game1) == HashGameState(game2));
}

TEST_CASE("Death and respawn determinism fuzz", "[determinism][death]") {
  // Stress-test the death/respawn path with aggressive inputs over many frames.
  // Uses low health so deaths occur frequently, and biases inputs toward combat
  // (fire held + movement) to maximize projectile/death interactions.
  //
  // This targets the known desync risk in beginRespawn() where the RNG-based
  // position search depends on level pixel state.

  PrecomputeTables();

  auto common = std::make_shared<Common>();
  FsNode const kTcRoot(FsNode("data") / "TC" / "openliero");
  common->load(kTcRoot);

  auto settings = std::make_shared<Settings>();
  settings->lives = 50;        // Many lives = many death/respawn cycles
  settings->loading_time = 0;  // Fast weapon reload
  settings->random_level = true;
  settings->game_mode = Settings::kGmKillEmAll;
  settings->blood = 100;

  // Use multiple seeds to cover different level layouts
  uint32_t const kSeeds[] = {42, 1337, 99999, 0xDEAD, 0xBEEF};

  for (uint32_t const kSeed : kSeeds) {
    auto sp_a = std::make_shared<NullSoundPlayer>();
    auto sp_b = std::make_shared<NullSoundPlayer>();

    Game game_a(common, settings, sp_a);
    Game game_b(common, settings, sp_b);

    game_a.rand.Seed(kSeed);
    game_b.rand.Seed(kSeed);

    for (int idx = 0; idx < 2; ++idx) {
      auto w_a = std::make_shared<Worm>();
      w_a->settings = settings->worm_settings[idx];
      w_a->health = 25;  // Low health for quick deaths
      w_a->index = idx;
      w_a->stats_x = idx == 0 ? 0 : 218;
      game_a.AddWorm(w_a);

      auto w_b = std::make_shared<Worm>();
      w_b->settings = settings->worm_settings[idx];
      w_b->health = 25;
      w_b->index = idx;
      w_b->stats_x = idx == 0 ? 0 : 218;
      game_b.AddWorm(w_b);
    }

    game_a.AddViewport(new Viewport(Rect(0, 0, 158, 158), 0, 504, 350));
    game_a.AddViewport(new Viewport(Rect(160, 0, 318, 158), 1, 504, 350));
    game_b.AddViewport(new Viewport(Rect(0, 0, 158, 158), 0, 504, 350));
    game_b.AddViewport(new Viewport(Rect(160, 0, 318, 158), 1, 504, 350));

    game_a.level.GenerateFromSettings(*common, *settings, game_a.rand);
    game_b.level.GenerateFromSettings(*common, *settings, game_b.rand);

    for (auto const& w : game_a.worms) {
      w->InitWeapons(game_a);
    }
    for (auto const& w : game_b.worms) {
      w->InitWeapons(game_b);
    }

    game_a.paused = false;
    game_b.paused = false;
    game_a.StartGame();
    game_b.StartGame();
    game_a.ResetWorms();
    game_b.ResetWorms();

    Rand input_rng(kSeed * 2654435761U + 1);

    constexpr int kNumFrames = 5000;
    int death_count = 0;

    for (int frame = 0; frame < kNumFrames; ++frame) {
      for (int idx = 0; idx < 2; ++idx) {
        uint32_t input = input_rng() & 0x7f;
        // Bias toward combat: 60% chance fire is held
        if ((input_rng() % 10) < 6) {
          input |= (1 << 4);  // Fire bit
        }
        // 40% chance of movement toward opponent
        if ((input_rng() % 10) < 4) {
          input |= (1 << (idx == 0 ? 1 : 0));  // Left/Right toward other
        }

        game_a.worms[idx]->control_states.Unpack(input);
        game_b.worms[idx]->control_states.Unpack(input);
      }

      game_a.ProcessFrame();
      game_b.ProcessFrame();

      // Track deaths for info output — killedTimer is set to 150 on death,
      // then decremented each frame, so 149 means "just died this frame"
      for (auto const& w : game_a.worms) {
        if (!w->visible && w->killed_timer == Worm::kKilledTimerInitial - 1) {
          ++death_count;
        }
      }

      // Check state every frame
      uint32_t const kHashA = HashGameState(game_a);
      uint32_t const kHashB = HashGameState(game_b);

      if (kHashA != kHashB) {
        // Identify which component diverged
        uint32_t const kRngA = game_a.rand.last;
        uint32_t const kRngB = game_b.rand.last;
        bool const kRngMatch = (kRngA == kRngB);

        bool worms_match = true;
        for (size_t i = 0; i < game_a.worms.size(); ++i) {
          if (game_a.worms[i]->pos.x != game_b.worms[i]->pos.x ||
              game_a.worms[i]->pos.y != game_b.worms[i]->pos.y ||
              game_a.worms[i]->visible != game_b.worms[i]->visible ||
              game_a.worms[i]->health != game_b.worms[i]->health ||
              game_a.worms[i]->killed_timer != game_b.worms[i]->killed_timer) {
            worms_match = false;
            break;
          }
        }

        // Check level pixels
        bool level_match = true;
        for (int i = 0; i < game_a.level.width * game_a.level.height; ++i) {
          if (game_a.level.material_id[i] != game_b.level.material_id[i]) {
            level_match = false;
            break;
          }
        }

        // Check object counts
        int nobjects_a = 0;
        int nobjects_b = 0;
        {
          auto r = game_a.nobjects.All();
          NObject const* n = nullptr;
          while ((n = r.Next())) {
            ++nobjects_a;
          }
        }
        {
          auto r = game_b.nobjects.All();
          NObject const* n = nullptr;
          while ((n = r.Next())) {
            ++nobjects_b;
          }
        }

        int bobjects_a = 0;
        int bobjects_b = 0;
        {
          auto br = game_a.bobjects.Begin();
          for (; br != game_a.bobjects.End(); ++br) {
            ++bobjects_a;
          }
        }
        {
          auto br = game_b.bobjects.Begin();
          for (; br != game_b.bobjects.End(); ++br) {
            ++bobjects_b;
          }
        }

        // Deep compare BObjects
        bool bobjects_match = true;
        {
          auto br_a = game_a.bobjects.Begin();
          auto br_b = game_b.bobjects.Begin();
          int idx = 0;
          for (; br_a != game_a.bobjects.End() && br_b != game_b.bobjects.End();
               ++br_a, ++br_b, ++idx) {
            if (br_a->pos.x != br_b->pos.x || br_a->pos.y != br_b->pos.y) {
              INFO("  BObject[" << idx << "] pos differs: A=(" << br_a->pos.x << "," << br_a->pos.y
                                << ") B=(" << br_b->pos.x << "," << br_b->pos.y << ")");
              bobjects_match = false;
              break;
            }
          }
        }

        // Deep compare NObjects
        bool nobjects_match = true;
        {
          auto r_a = game_a.nobjects.All();
          auto r_b = game_b.nobjects.All();
          NObject const* n_a = nullptr;
          NObject const* n_b = nullptr;
          int idx = 0;
          while ((n_a = r_a.Next()) && (n_b = r_b.Next())) {
            if (n_a->pos.x != n_b->pos.x || n_a->pos.y != n_b->pos.y || n_a->vel.x != n_b->vel.x ||
                n_a->vel.y != n_b->vel.y) {
              INFO("  NObject[" << idx << "] differs: A pos=(" << n_a->pos.x << "," << n_a->pos.y
                                << ") vel=(" << n_a->vel.x << "," << n_a->vel.y << ") B pos=("
                                << n_b->pos.x << "," << n_b->pos.y << ") vel=(" << n_b->vel.x << ","
                                << n_b->vel.y << ")");
              nobjects_match = false;
              break;
            }
            ++idx;
          }
        }

        // Deep compare WObjects
        bool wobjects_match = true;
        int wobjects_a = 0;
        int wobjects_b = 0;
        {
          auto r_a = game_a.wobjects.All();
          auto r_b = game_b.wobjects.All();
          WObject const* w_a = nullptr;
          WObject const* w_b = nullptr;
          int idx = 0;
          while ((w_a = r_a.Next())) {
            ++wobjects_a;
            (void)w_a;
          }
          while ((w_b = r_b.Next())) {
            ++wobjects_b;
            (void)w_b;
          }
          r_a = game_a.wobjects.All();
          r_b = game_b.wobjects.All();
          while ((w_a = r_a.Next()) && (w_b = r_b.Next())) {
            if (w_a->pos.x != w_b->pos.x || w_a->pos.y != w_b->pos.y || w_a->vel.x != w_b->vel.x ||
                w_a->vel.y != w_b->vel.y || w_a->cur_frame != w_b->cur_frame ||
                w_a->time_left != w_b->time_left) {
              INFO("  WObject[" << idx << "] differs: A pos=(" << w_a->pos.x << "," << w_a->pos.y
                                << ") tl=" << w_a->time_left << " B pos=(" << w_b->pos.x << ","
                                << w_b->pos.y << ") tl=" << w_b->time_left);
              wobjects_match = false;
              break;
            }
            ++idx;
          }
        }

        // Deep compare SObjects
        bool sobjects_match = true;
        int sobjects_a = 0;
        int sobjects_b = 0;
        {
          auto r_a = game_a.sobjects.All();
          auto r_b = game_b.sobjects.All();
          SObject const* s_a = nullptr;
          SObject const* s_b = nullptr;
          int idx = 0;
          while ((s_a = r_a.Next())) {
            ++sobjects_a;
          }
          while ((s_b = r_b.Next())) {
            ++sobjects_b;
          }
          r_a = game_a.sobjects.All();
          r_b = game_b.sobjects.All();
          while ((s_a = r_a.Next()) && (s_b = r_b.Next())) {
            if (s_a->id != s_b->id || s_a->cur_frame != s_b->cur_frame) {
              INFO("  SObject[" << idx << "] differs: A id=" << s_a->id
                                << " frame=" << s_a->cur_frame << " B id=" << s_b->id
                                << " frame=" << s_b->cur_frame);
              sobjects_match = false;
              break;
            }
            ++idx;
          }
        }

        // Deep compare Bonuses
        bool bonuses_match = true;
        int bonuses_a = 0;
        int bonuses_b = 0;
        {
          auto r_a = game_a.bonuses.All();
          auto r_b = game_b.bonuses.All();
          Bonus const* b_a = nullptr;
          Bonus const* b_b = nullptr;
          int idx = 0;
          while ((b_a = r_a.Next())) {
            ++bonuses_a;
          }
          while ((b_b = r_b.Next())) {
            ++bonuses_b;
          }
          r_a = game_a.bonuses.All();
          r_b = game_b.bonuses.All();
          while ((b_a = r_a.Next()) && (b_b = r_b.Next())) {
            if (b_a->x != b_b->x || b_a->y != b_b->y || b_a->timer != b_b->timer ||
                b_a->weapon != b_b->weapon) {
              INFO("  Bonus[" << idx << "] differs");
              bonuses_match = false;
              break;
            }
            ++idx;
          }
        }

        // Deep compare worm weapons
        bool weapons_match = true;
        for (size_t wi = 0; wi < game_a.worms.size(); ++wi) {
          auto const& w_a = game_a.worms[wi];
          auto const& w_b = game_b.worms[wi];
          for (int i = 0; i < NUM_WEAPONS; ++i) {
            if (w_a->weapons[i].ammo != w_b->weapons[i].ammo ||
                w_a->weapons[i].delay_left != w_b->weapons[i].delay_left ||
                w_a->weapons[i].loading_left != w_b->weapons[i].loading_left) {
              INFO("  Worm[" << wi << "].weapons[" << i << "] differs: A ammo="
                             << w_a->weapons[i].ammo << " delay=" << w_a->weapons[i].delay_left
                             << " loading=" << w_a->weapons[i].loading_left << " B ammo="
                             << w_b->weapons[i].ammo << " delay=" << w_b->weapons[i].delay_left
                             << " loading=" << w_b->weapons[i].loading_left);
              weapons_match = false;
            }
          }
        }

        INFO("Desync at frame " << frame << " (seed=" << kSeed << ", deaths=" << death_count << ")"
                                << "\n  RNG match=" << kRngMatch << " Worms match=" << worms_match
                                << " Level match=" << level_match << "\n  NObjects: A="
                                << nobjects_a << " B=" << nobjects_b << " match=" << nobjects_match
                                << "\n  BObjects: A=" << bobjects_a << " B=" << bobjects_b
                                << " match=" << bobjects_match << "\n  WObjects: A=" << wobjects_a
                                << " B=" << wobjects_b << " match=" << wobjects_match
                                << "\n  SObjects: A=" << sobjects_a << " B=" << sobjects_b
                                << " match=" << sobjects_match << "\n  Bonuses: A=" << bonuses_a
                                << " B=" << bonuses_b << " match=" << bonuses_match
                                << "\n  Weapons match=" << weapons_match << "\n  hashA=0x"
                                << std::hex << kHashA << " hashB=0x" << kHashB);
        REQUIRE(kHashA == kHashB);
      }

      // Stop if game is over
      if (game_a.IsGameOver()) {
        break;
      }
    }

    INFO("Seed " << kSeed << " completed: " << death_count << " deaths observed");
    REQUIRE(death_count > 0);  // Sanity check: we actually tested deaths
  }
}
