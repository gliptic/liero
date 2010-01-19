
#include "timeBehavior.hpp"

#include "menu.hpp"
#include "menuItem.hpp"
#include "../common.hpp"
#include "../text.hpp"

void TimeBehavior::onUpdate(Menu& menu, int item)
{
	MenuItem& i = menu.items[item];
	i.value = timeToString(v);
	i.hasValue = true;
}
