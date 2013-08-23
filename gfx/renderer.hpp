#ifndef LIERO_GFX_RENDERER_HPP
#define LIERO_GFX_RENDERER_HPP

#include "../common.hpp"
#include "../rand.hpp"
#include "bitmap.hpp"

struct Renderer
{
	Renderer()
	: fadeValue(0)
	{
	}

	void init();
	void clear();
	void loadPalette(Common const& common);

	Rand rand; // PRNG for things that don't affect the game
	Bitmap screenBmp;
	Palette pal, origpal;
	int fadeValue;
};

#endif // LIERO_GFX_RENDERER_HPP
