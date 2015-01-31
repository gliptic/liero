#include "renderer.hpp"
#include "blit.hpp"
#include "../common.hpp"

void Renderer::init()
{
	screenBmp.alloc(640, 480);
}

void Renderer::loadPalette(Common const& common)
{
	origpal = common.exepal;
	pal = origpal;
}

void Renderer::clear()
{
	fill(screenBmp, 0);
}
