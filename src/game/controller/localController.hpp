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
  LocalController(const std::shared_ptr<Common>& common, const std::shared_ptr<Settings>& settings);
  ~LocalController() override;
  void OnKey(int key, bool key_state) override;

  // Called when the controller loses focus. When not focused, it will not receive key events among
  // other things.
  void Unfocus() override;
  // Called when the controller gets focus.
  void Focus() override;
  bool Process() override;
  void Draw(Renderer& renderer, bool use_spectator_viewports) override;
  void ChangeState(GameState new_state);
  void EndRecord();
  void SwapLevel(Level& new_level) override;
  Level* CurrentLevel() override;
  Game* CurrentGame() override;
  bool Running() override;

  Game game;
  std::unique_ptr<WeaponSelection> ws;
  GameState state{kStateInitial};
  int fade_value{0};
  bool going_to_menu{false};
  std::unique_ptr<ReplayWriter> replay;

  // Per-worm key repeat counters for weapon selection
  static constexpr int kKeyRepeatInitial = 12;
  static constexpr int kKeyRepeatInterval = 3;
  std::array<std::array<uint16_t, 7>, 2> worm_held_frames{};
};
