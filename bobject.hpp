#ifndef LIERO_BOBJECT_HPP
#define LIERO_BOBJECT_HPP

#include "math.hpp"
#include "fastObjectList.hpp"

struct Game;

struct BObject
{
	bool process(Game& game);
	
	fixed x, y;
	fixed velX, velY;
	int color;
};

#endif // LIERO_BOBJECT_HPP
