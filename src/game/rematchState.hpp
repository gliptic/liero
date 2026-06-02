#pragma once

#include "menu/menu.hpp"
#include "state.hpp"

#include <string>

struct Game;

// Post-stats rematch screen for multiplayer games.
// Lets the host change the level and both players signal readiness.
// When both are ready, starts a new game.
struct RematchState : AppState {
  enum Items {
    kRmLevel,
    kRmReady,
    kRmDisconnect,
  };

  RematchState(Game& last_game);

  void Enter() override;
  void HandleEvent(SDL_Event& ev) override;
  bool Update() override;
  void Draw() override;
  bool WantsMenuFlip() const override { return true; }

 private:
  std::string LevelDisplayName() const;
  void UpdateMenuItems();

  Game& lastGame_;
  Menu menu_;
  bool levelSelectorOpen_ = false;

  // Snapshot of level settings to detect changes from LevelSelectorState
  bool prevRandomLevel_ = false;
  std::string prevLevelFile_;
};
