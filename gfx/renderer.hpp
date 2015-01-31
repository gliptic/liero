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
 	// Resolution to render at. In effect, this determines how much of screenBmp is used. These may
 	// not be larger than screenBmp.x and screenBmp.y
	int renderResX = 320;
	int renderResY = 200;
	Palette pal, origpal;
	int fadeValue;
};

#endif // LIERO_GFX_RENDERER_HPP
