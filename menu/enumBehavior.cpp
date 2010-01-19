
#include "enumBehavior.hpp"

#include "menu.hpp"
#include "menuItem.hpp"
#include "../common.hpp"
#include "../text.hpp"
#include "../sfx.hpp"

bool EnumBehavior::onLeftRight(Menu& menu, int item, int dir)
{
	if(brokenLeftRight)
		return false; // Left/right doesn't work for this item
	if(dir > 0)
		sfx.play(25);
	else
		sfx.play(26);
		
	change(menu, item, dir);
		
	return false;
}

int EnumBehavior::onEnter(Menu& menu, int item)
{
	sfx.play(27);
	
	change(menu, item, 1);
	return -1;
}

void EnumBehavior::change(Menu& menu, int item, int dir)
{
	uint32_t range = max - min + 1;
	uint32_t newV = ((v + dir + range - min) % range) + min;
	
	if(newV != v)
	{
		v = newV;
		onUpdate(menu, item);
	}
}

void EnumBehavior::onUpdate(Menu& menu, int item)
{
	MenuItem& i = menu.items[item];
	i.value = toString(v);
	i.hasValue = true;
}
