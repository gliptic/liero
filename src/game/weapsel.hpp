#pragma once

#include "game.hpp"
#include "gfx/renderer.hpp"
#include "menu/menu.hpp"

struct Game;

struct WeaponSelection {
  WeaponSelection(Game& game);

  void Draw(Renderer& renderer, GameState state, bool use_spectator_viewports);
  void DrawNormalViewports(Renderer& renderer, GameState state);
  void DrawSpectatorViewports(Renderer& renderer, GameState state);
  bool ProcessFrame();
  void Finalize();

  void Focus();
  void Unfocus();

  Game& game;

  int enabled_weaps;
  int fade_value;
  std::vector<bool> is_ready;
  std::vector<Menu> menus;
  bool cached_background, cached_spectator_background;
  bool focused;
};

// void selectWeapons(Game& game);
