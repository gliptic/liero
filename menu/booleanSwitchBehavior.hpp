#ifndef UUID_0F083A4D564C4D79CA6387B1D0F1901E
#define UUID_0F083A4D564C4D79CA6387B1D0F1901E

#include "itemBehavior.hpp"
#include <functional>

struct Common;
struct Menu;

struct BooleanSwitchBehavior : ItemBehavior
{
	BooleanSwitchBehavior(Common& common, bool& v)
	: common(common), v(v)
	, set([&](bool newV) { v = newV; })
	{
		
	}

	BooleanSwitchBehavior(Common& common, bool& v, std::function<void(bool)> set)
	: common(common), v(v)
	, set(set)
	{
		
	}

	std::function<void(bool)> set;
	
	bool onLeftRight(Menu& menu, int item, int dir);
	int onEnter(Menu& menu, int item);
	void onUpdate(Menu& menu, int item);
	
	Common& common;
	bool& v;
};

#endif // UUID_0F083A4D564C4D79CA6387B1D0F1901E
