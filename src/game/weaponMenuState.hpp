#pragma once

#include "state.hpp"
#include "menu/menu.hpp"

// Weapon availability settings state.
struct WeaponMenuState : AppState
{
	WeaponMenuState();

	void enter() override;
	void handleEvent(SDL_Event& ev) override;
	bool update() override;
	void draw() override;

private:
	std::unique_ptr<Menu> weaponMenu_;
	bool done_ = false;
};
