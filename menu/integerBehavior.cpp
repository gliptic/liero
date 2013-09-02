
#include "integerBehavior.hpp"

#include "menu.hpp"
#include "menuItem.hpp"
#include "../sfx.hpp"
#include "../gfx.hpp"
#include "../common.hpp"
#include "../text.hpp"

bool IntegerBehavior::onLeftRight(Menu& menu, MenuItem& item, int dir)
{
	//if(gfx.menuCyclic != 0)
	if ((gfx.menuCycles % scrollInterval) == 0)
		return true;
		
	int newV = v;
	if((dir < 0 && newV > min)
	|| (dir > 0 && newV < max))
	{
		newV += dir * step;
	}
		
	if(newV != v)
	{
		v = newV;
		onUpdate(menu, item);
	}
	
	return true;
}

int IntegerBehavior::onEnter(Menu& menu, MenuItem& item)
{
	sfx.play(common, 27);
	
	if(!allowEntry)
		return -1; // Not allowed
		
	int x, y;
	if(menu.itemPosition(item, x, y))
	{
		x += menu.valueOffsetX;
		int digits = 1 + int(std::floor(std::log10(double(max))));
		gfx.inputInteger(v, min, max, digits, x + 2, y);
		onUpdate(menu, item);
	}
	return -1;
}

void IntegerBehavior::onUpdate(Menu& menu, MenuItem& item)
{
	item.value = toString(v);
	item.hasValue = true;
	if(percentage)
		item.value += "%";
}
