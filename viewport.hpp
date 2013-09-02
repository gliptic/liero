#ifndef LIERO_VIEWPORT_HPP
#define LIERO_VIEWPORT_HPP

#include "rect.hpp"
#include "worm.hpp"
#include "rand.hpp"
#include <ctime>

struct Game;
struct Renderer;

struct Viewport
{
	Viewport(Rect rect, int wormIdx, int inGameX, int levwidth, int levheight)
	: wormIdx(wormIdx)
	, bannerY(-8)
	, inGameX(inGameX)
	, rect(rect)
	{
		rand.seed((uint32_t)std::clock());
		maxX = levwidth - rect.width();
		maxY = levheight - rect.height();
		centerX = rect.width() >> 1;
		centerY = rect.height() >> 1;
		x = 0;
		y = 0;
		shake = 0;
	}
	
	Viewport()
	{
		
	}
	
	int x, y;
	int shake;
	int maxX, maxY;
	int centerX, centerY;
	int wormIdx;
	int bannerY;
	int inGameX; // 0 for first, 218 for second
	Rand rand;
	Rect rect;

	
	void setCenter(int x, int y)
	{
		this->x = x - centerX;
		this->y = y - centerY;
	}
	
	void scrollTo(int destX, int destY, int iter)
	{
		for(int c = 0; c < iter; c++)
		{
			if(x < destX - centerX) ++x;
			else if(x > destX - centerX) --x;

			if(y < destY - centerY) ++y;
			else if(y > destY - centerY) --y;
		}
	}
	
	
	void draw(Game& game, Renderer& renderer, bool isReplay);
	void process(Game& game);
};

#endif // LIERO_VIEWPORT_HPP
