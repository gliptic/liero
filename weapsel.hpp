#ifndef LIERO_WEAPSEL_HPP
#define LIERO_WEAPSEL_HPP

#include "menu.hpp"

struct Game;

struct WeaponSelection
{
	WeaponSelection(Game& game);
	
	void draw();
	bool processFrame();
	void finalize();
	
	void focus();
	void unfocus();
	
	Game& game;
	
	int enabledWeaps;
	int fadeValue;
	std::vector<bool> isReady;
	std::vector<Menu> menus;
	bool cachedBackground;
	bool focused;
};

//void selectWeapons(Game& game);

#endif // LIERO_WEAPSEL_HPP
