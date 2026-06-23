// Shared headless game builder for integration tests.
//
// ResetWorms() sets worm health from WormSettings::health, so cfg.health is
// applied after that call to actually take effect.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>

#include "game.hpp"
#include "level.hpp"
#include "math.hpp"
#include "mixer/player.hpp"
#include "viewport.hpp"
#include "weapon.hpp"
#include "worm.hpp"

struct HeadlessGameConfig {
  uint32_t seed{42};
  int game_mode{Settings::kGmKillEmAll};
  int lives{3};
  int health{25};  // per-worm health after ResetWorms; 0 → keep WormSettings::health default
  int worm_count{2};
};

// Returns a fully-initialized Game ready for ProcessFrame(), following the
// DualGameFixture setup order from test_determinism.cpp.
inline std::unique_ptr<Game> MakeHeadlessGame(HeadlessGameConfig const& cfg = {}) {
  PrecomputeTables();

  auto common = std::make_shared<Common>();
  FsNode const kTcRoot(FsNode("data") / "TC" / "openliero");
  common->load(kTcRoot);

  auto settings = std::make_shared<Settings>();
  settings->lives = cfg.lives;
  settings->loading_time = 0;
  settings->random_level = true;
  settings->game_mode = cfg.game_mode;

  auto sp = std::make_shared<NullSoundPlayer>();
  auto game = std::make_unique<Game>(common, settings, sp, /*install_global_sound_player=*/false);
  game->rand.Seed(cfg.seed);

  for (int idx = 0; idx < cfg.worm_count; ++idx) {
    auto w = std::make_shared<Worm>();
    w->settings = settings->worm_settings[idx];
    w->health = w->settings->health;
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

  if (cfg.health > 0) {
    for (auto const& w : game->worms) {
      w->health = cfg.health;
    }
  }

  return game;
}

struct RunResult {
  int frames_elapsed;
  bool reached_game_over;
};

// Feeds scripted input until IsGameOver() fires or max_frames is exhausted.
// input_fn(worm_index, frame) returns a 7-bit control byte (matches Unpack).
using InputFn = std::function<uint8_t(int worm_index, int frame)>;

inline RunResult RunToCompletion(Game& game, InputFn const& input_fn, int max_frames = 500000) {
  for (int frame = 0; frame < max_frames; ++frame) {
    for (size_t idx = 0; idx < game.worms.size(); ++idx) {
      game.worms[idx]->control_states.Unpack(input_fn(static_cast<int>(idx), frame));
    }
    game.ProcessFrame();
    if (game.IsGameOver()) {
      return {.frames_elapsed = frame + 1, .reached_game_over = true};
    }
  }
  return {.frames_elapsed = max_frames, .reached_game_over = false};
}
