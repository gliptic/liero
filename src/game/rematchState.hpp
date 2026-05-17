#pragma once

#include "state.hpp"

#include <string>

struct Game;

// Post-stats rematch screen for multiplayer games.
// Lets the host change the level and both players signal readiness.
// When both are ready, starts a new game.
struct RematchState : AppState
{
	RematchState(Game& lastGame);

	void enter() override;
	void handleEvent(SDL_Event& ev) override;
	bool update() override;
	void draw() override;
	bool wantsMenuFlip() const override { return false; }

private:
	std::string levelDisplayName() const;

	Game& lastGame_;
	bool levelSelectorOpen_ = false;

	// Snapshot of level settings to detect changes from LevelSelectorState
	bool prevRandomLevel_ = false;
	std::string prevLevelFile_;
};
