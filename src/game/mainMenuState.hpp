#pragma once

#include "state.hpp"

// The main menu state, with sub-menus as child states.
struct MainMenuState : AppState {
  MainMenuState();

  void enter() override;
  void handleEvent(SDL_Event& ev) override;
  bool update() override;
  void draw() override;

  // The menu item that was selected, or -1 if still active.
  int selection() const { return selected_; }

  // True when fading out
  bool isFadingOut() const { return phase_ == Phase::FadingOut; }

 private:
  enum class Phase { Active, FadingOut };

  Phase phase_ = Phase::Active;
  int selected_ = -1;
  int startItemId_ = 0;
};
