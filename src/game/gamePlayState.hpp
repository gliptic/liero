#pragma once

#include "state.hpp"

// Wraps the game loop (controller->process() per frame) as an AppState.
// Pushed onto the state stack when the player starts or resumes a game.
// Pops itself when the controller signals the game is over (process() returns false).
struct GamePlayState : AppState {
  void enter() override;
  void handleEvent(SDL_Event& ev) override;
  bool update() override;
  void draw() override;
  bool wantsMenuFlip() const override { return false; }
};
