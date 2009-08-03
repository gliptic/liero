#ifndef LIERO_BONUS_HPP
#define LIERO_BONUS_HPP

#include "math.hpp"
#include "objectList.hpp"
#include "exactObjectList.hpp"

struct Game;

struct Bonus : ExactObjectListBase
{
	Bonus()
	: frame(-1)
	{
	}
	
	fixed x;
	fixed y;
	fixed velY;
	int frame;
	int timer;
	int weapon;
		
	void process(Game& game);
};

#endif // LIERO_BONUS_HPP
