#ifndef LIERO_BOBJECT_HPP
#define LIERO_BOBJECT_HPP

#include "math.hpp"
#include "fastObjectList.hpp"

struct Game;

struct BObject
{
	bool process(Game& game);

	fixedvec pos, vel;
	int color;
};

#endif // LIERO_BOBJECT_HPP
