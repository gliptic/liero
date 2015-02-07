#ifndef LIERO_SPECTATORVIEWPORT_HPP
#define LIERO_SPECTATORVIEWPORT_HPP

#include "worm.hpp"
#include "rand.hpp"
#include "viewport.hpp"
#include <gvl/math/rect.hpp>
#include <ctime>

struct Game;
struct Renderer;

struct SpectatorViewport : Viewport
{
	SpectatorViewport(gvl::rect rect, int levwidth, int levheight)
	: Viewport(rect, 0, levwidth, levheight)
	{
	}

	void draw(Game& game, Renderer& renderer, bool isReplay);
	void process(Game& game);
};

#endif // LIERO_SPECTATORVIEWPORT_HPP
