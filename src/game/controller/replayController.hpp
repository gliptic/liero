#pragma once

#include "commonController.hpp"

#include "../game.hpp"
#include "../gfx.hpp"
#include "../keys.hpp"

#include <ctime>
#include "../console.hpp"
#include "../mixer/mixer.hpp"
#include "../replay.hpp"
#include "../weapsel.hpp"
#include "../worm.hpp"

struct Game;

struct ReplayController : CommonController {
  ReplayController(std::shared_ptr<Common> common, std::unique_ptr<io::Reader> source);

  bool IsReplay() override { return true; };
  void OnKey(int key, bool key_state) override;
  // Called when the controller loses focus. When not focused, it will not receive key events among
  // other things.
  void Unfocus() override;
  // Called when the controller gets focus.
  void Focus() override;
  bool Process() override;
  void Draw(Renderer& renderer, bool use_spectator_viewports) override;
  void ChangeState(GameState new_state);
  void SwapLevel(Level& new_level) override;
  Level* CurrentLevel() override;
  Game* CurrentGame() override;
  bool Running() override;

  std::unique_ptr<Game> game;

  std::unique_ptr<Game> initial_game;
  std::size_t initial_reader_pos = 0;

  GameState state{kStateInitial};
  int fade_value{0};
  bool going_to_menu{false};
  std::unique_ptr<ReplayReader> replay;
  std::shared_ptr<Common> common;
};
