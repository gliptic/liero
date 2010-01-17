#ifndef LIERO_MENU_INTEGER_BEHAVIOUR_HPP
#define LIERO_MENU_INTEGER_BEHAVIOUR_HPP

#include "itemBehaviour.hpp"

struct Common;
struct Menu;

struct IntegerBehavior : ItemBehavior
{
	IntegerBehavior(Common& common, int& v, int min, int max, int step = 1, bool percentage = false)
	: common(common), v(v)
	, min(min), max(max), step(step)
	, percentage(percentage)
	, allowEntry(true)
	{
	}
	
	bool onLeftRight(Menu& menu, int item, int dir);
	int onEnter(Menu& menu, int item);
	void onUpdate(Menu& menu, int item);
	
	
	
	Common& common;
	int& v;
	int min, max, step;
	bool percentage;
	bool allowEntry;
};

#endif // LIERO_MENU_INTEGER_BEHAVIOUR_HPP
