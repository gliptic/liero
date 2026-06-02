#pragma once

#include "commonController.hpp"

#include "../game.hpp"

#include <array>
#include <ctime>
#include "../console.hpp"
#include "../replay.hpp"
#include "../weapsel.hpp"
#include "../worm.hpp"

struct WeaponSelection;
struct ReplayWriter;

struct LocalController : CommonController {
  LocalController(std::shared_ptr<Common> common, std::shared_ptr<Settings> settings);
  ~LocalController();
  void OnKey(int key, bool key_state);

  // Called when the controller loses focus. When not focused, it will not receive key events among
  // other things.
  void Unfocus();
  // Called when the controller gets focus.
  void Focus();
  bool Process();
  void Draw(Renderer& renderer, bool use_spectator_viewports);
  void ChangeState(GameState new_state);
  void EndRecord();
  void SwapLevel(Level& new_level);
  Level* CurrentLevel();
  Game* CurrentGame();
  bool Running();

  Game game;
  std::unique_ptr<WeaponSelection> ws;
  GameState state;
  int fade_value;
  bool going_to_menu;
  std::unique_ptr<ReplayWriter> replay;

  // Per-worm key repeat counters for weapon selection
  static constexpr int kKeyRepeatInitial = 12;
  static constexpr int kKeyRepeatInterval = 3;
  std::array<std::array<uint16_t, 7>, 2> worm_held_frames{};
};
