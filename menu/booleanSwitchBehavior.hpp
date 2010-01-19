#ifndef LIERO_MENU_BOOLEAN_SWITCH_BEHAVIOUR_HPP
#define LIERO_MENU_BOOLEAN_SWITCH_BEHAVIOUR_HPP

#include "itemBehavior.hpp"

struct Common;
struct Menu;

struct BooleanSwitchBehavior : ItemBehavior
{
	BooleanSwitchBehavior(Common& common, bool& v)
	: common(common), v(v)
	{
	}
	
	bool onLeftRight(Menu& menu, int item, int dir);
	int onEnter(Menu& menu, int item);
	void onUpdate(Menu& menu, int item);
	
	Common& common;
	bool& v;
};

#endif // LIERO_MENU_BOOLEAN_SWITCH_BEHAVIOUR_HPP
