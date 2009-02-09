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
	
	Game& game;
	
	int enabledWeaps;
	int fadeValue;
	int curSel[2];
	bool isReady[2];
	Menu menus[2];
	bool cachedBackground;
};

//void selectWeapons(Game& game);

#endif // LIERO_WEAPSEL_HPP
