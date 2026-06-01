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

  bool isReplay() { return true; };
  void onKey(int key, bool keyState);
  // Called when the controller loses focus. When not focused, it will not receive key events among
  // other things.
  void unfocus();
  // Called when the controller gets focus.
  void focus();
  bool process();
  void draw(Renderer& renderer, bool useSpectatorViewports);
  void changeState(GameState newState);
  void swapLevel(Level& newLevel);
  Level* currentLevel();
  Game* currentGame();
  bool running();

  std::unique_ptr<Game> game;

  std::unique_ptr<Game> initialGame;
  std::size_t initialReaderPos = 0;

  GameState state;
  int fadeValue;
  bool goingToMenu;
  std::unique_ptr<ReplayReader> replay;
  std::shared_ptr<Common> common;
};
