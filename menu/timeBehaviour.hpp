#ifndef LIERO_MENU_TIME_BEHAVIOUR_HPP
#define LIERO_MENU_TIME_BEHAVIOUR_HPP

#include "integerBehaviour.hpp"

struct Common;
struct Menu;

struct TimeBehavior : IntegerBehavior
{
	TimeBehavior(Common& common, int& v, int min, int max, int step = 1, bool percentage = false)
	: IntegerBehavior(common, v, min, max, step, percentage)
	{
	}
	
	void onUpdate(Menu& menu, int item);
};


#endif // LIERO_MENU_TIME_BEHAVIOUR_HPP
