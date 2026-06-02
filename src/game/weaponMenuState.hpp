#pragma once

#include "menu/menu.hpp"
#include "state.hpp"

// Weapon availability settings state.
struct WeaponMenuState : AppState {
  WeaponMenuState();

  void Enter() override;
  void HandleEvent(SDL_Event& ev) override;
  bool Update() override;
  void Draw() override;

 private:
  std::unique_ptr<Menu> weaponMenu_;
  bool done_ = false;
};
