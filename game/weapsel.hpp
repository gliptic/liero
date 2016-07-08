#ifndef LIERO_WEAPSEL_HPP
#define LIERO_WEAPSEL_HPP

#include "game.hpp"
#include "menu/menu.hpp"
#include "gfx/renderer.hpp"

struct Game;

struct WeaponSelection
{
	WeaponSelection(Game& game);

	void draw(Renderer& renderer, GameState state, bool useSpectatorViewports);
	void drawNormalViewports(Renderer& renderer, GameState state);
	void drawSpectatorViewports(Renderer& renderer, GameState state);
	bool processFrame();
	void finalize();

	void focus();
	void unfocus();

	Game& game;

	int enabledWeaps;
	int fadeValue;
	std::vector<bool> isReady;
	std::vector<Menu> menus;
	bool cachedBackground, cachedSpectatorBackground;
	bool focused;
};

//void selectWeapons(Game& game);

#endif // LIERO_WEAPSEL_HPP
