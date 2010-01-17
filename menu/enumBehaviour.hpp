#ifndef LIERO_MENU_ENUM_BEHAVIOUR_HPP
#define LIERO_MENU_ENUM_BEHAVIOUR_HPP

#include "itemBehaviour.hpp"

#include <stdint.h>

struct Common;
struct Menu;

struct EnumBehavior : ItemBehavior
{
	EnumBehavior(Common& common, uint32_t& v, uint32_t min, uint32_t max, bool brokenLeftRight = false)
	: common(common), v(v)
	, min(min), max(max)
	, brokenLeftRight(brokenLeftRight)
	{
	}
	
	bool onLeftRight(Menu& menu, int item, int dir);
	int onEnter(Menu& menu, int item);
	void onUpdate(Menu& menu, int item);
	
	void change(Menu& menu, int item, int dir);
	
	Common& common;
	uint32_t& v;
	uint32_t min, max;
	bool brokenLeftRight;
};

#endif // LIERO_MENU_ENUM_BEHAVIOUR_HPP
