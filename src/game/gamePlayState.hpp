#pragma once

#include "state.hpp"

// Wraps the game loop (controller->process() per frame) as an AppState.
// Pushed onto the state stack when the player starts or resumes a game.
// Pops itself when the controller signals the game is over (process() returns false).
struct GamePlayState : AppState {
  void Enter() override;
  void HandleEvent(SDL_Event& ev) override;
  bool Update() override;
  void Draw() override;
  bool WantsMenuFlip() const override { return false; }
};
