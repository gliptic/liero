#pragma once

#include "state.hpp"

// The main menu state. This is a frame-stepped version of the former
// blocking Gfx::menuLoop(). Sub-menus (playerSettings, hiddenMenu, file
// selectors) are still called as blocking functions for now — they will
// be converted to states in subsequent phases.
struct MainMenuState : AppState
{
	MainMenuState();

	void enter() override;
	void handleEvent(SDL_Event& ev) override;
	bool update() override;
	void draw() override;

	// The menu item that was selected, or -1 if still active.
	int selection() const { return selected_; }

	// True when fading out (used by runMenu for menuFlip parameter)
	bool isFadingOut() const { return phase_ == Phase::FadingOut; }

private:
	enum class Phase { Active, FadingOut };

	Phase phase_ = Phase::Active;
	int selected_ = -1;
	int startItemId_ = 0;
};
