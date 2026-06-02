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

  bool IsReplay() { return true; };
  void OnKey(int key, bool key_state);
  // Called when the controller loses focus. When not focused, it will not receive key events among
  // other things.
  void Unfocus();
  // Called when the controller gets focus.
  void Focus();
  bool Process();
  void Draw(Renderer& renderer, bool use_spectator_viewports);
  void ChangeState(GameState new_state);
  void SwapLevel(Level& new_level);
  Level* CurrentLevel();
  Game* CurrentGame();
  bool Running();

  std::unique_ptr<Game> game;

  std::unique_ptr<Game> initial_game;
  std::size_t initial_reader_pos = 0;

  GameState state;
  int fade_value;
  bool going_to_menu;
  std::unique_ptr<ReplayReader> replay;
  std::shared_ptr<Common> common;
};
