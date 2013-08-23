
#include "booleanSwitchBehavior.hpp"

#include "menu.hpp"
#include "menuItem.hpp"
#include "../sfx.hpp"
#include "../common.hpp"

bool BooleanSwitchBehavior::onLeftRight(Menu& menu, int item, int dir)
{
	if(dir > 0)
		sfx.play(common, 25);
	else
		sfx.play(common, 26);

	set(!v);
	onUpdate(menu, item);
	return false;
}

int BooleanSwitchBehavior::onEnter(Menu& menu, int item)
{
	sfx.play(common, 27);
	set(!v);
	onUpdate(menu, item);
	return -1;
}

void BooleanSwitchBehavior::onUpdate(Menu& menu, int item)
{
	MenuItem& i = menu.items[item];
	i.value = common.texts.onoff[v];
	i.hasValue = true;
}
