
#include "timeBehavior.hpp"

#include "menu.hpp"
#include "menuItem.hpp"
#include "../common.hpp"
#include "../text.hpp"

void TimeBehavior::onUpdate(Menu& menu, MenuItem& item)
{
	item.value = frames ? timeToStringFrames(v) : timeToString(v);
	item.hasValue = true;
}
