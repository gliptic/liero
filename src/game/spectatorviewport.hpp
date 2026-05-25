#pragma once

#include "game.hpp"
#include "worm.hpp"
#include "rand.hpp"
#include "viewport.hpp"
#include "math/rect.hpp"
#include <ctime>

struct Renderer;

struct SpectatorViewport : Viewport
{
	SpectatorViewport(Rect rect, int levwidth, int levheight)
	: Viewport(rect, 0, levwidth, levheight)
	{
	}

	void draw(Game& game, Renderer& renderer, GameState state, bool isReplay);
	void process(Game& game);
};
