
#include "integerBehaviour.hpp"

#include "menu.hpp"
#include "menuItem.hpp"
#include "../sfx.hpp"
#include "../gfx.hpp"
#include "../common.hpp"
#include "../text.hpp"

bool IntegerBehavior::onLeftRight(Menu& menu, int item, int dir)
{
	if(gfx.menuCyclic != 0)
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

int IntegerBehavior::onEnter(Menu& menu, int item)
{
	sfx.play(27);
	
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

void IntegerBehavior::onUpdate(Menu& menu, int item)
{
	MenuItem& i = menu.items[item];
	i.value = toString(v);
	i.hasValue = true;
	if(percentage)
		i.value += "%";
}
