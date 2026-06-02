#pragma once

#include "state.hpp"

// The main menu state, with sub-menus as child states.
struct MainMenuState : AppState {
  MainMenuState();

  void Enter() override;
  void HandleEvent(SDL_Event& ev) override;
  bool Update() override;
  void Draw() override;

  // The menu item that was selected, or -1 if still active.
  int Selection() const { return selected_; }

  // True when fading out
  bool IsFadingOut() const { return phase_ == Phase::kFadingOut; }

 private:
  enum class Phase { kActive, kFadingOut };

  Phase phase_ = Phase::kActive;
  int selected_ = -1;
  int startItemId_ = 0;
};
