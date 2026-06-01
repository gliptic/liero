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
    RmLevel,
    RmReady,
    RmDisconnect,
  };

  RematchState(Game& lastGame);

  void enter() override;
  void handleEvent(SDL_Event& ev) override;
  bool update() override;
  void draw() override;
  bool wantsMenuFlip() const override { return true; }

 private:
  std::string levelDisplayName() const;
  void updateMenuItems();

  Game& lastGame_;
  Menu menu_;
  bool levelSelectorOpen_ = false;

  // Snapshot of level settings to detect changes from LevelSelectorState
  bool prevRandomLevel_ = false;
  std::string prevLevelFile_;
};
