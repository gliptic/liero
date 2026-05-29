#include "integerBehavior.hpp"

#include "menu.hpp"
#include "menuItem.hpp"
#include "../mixer/player.hpp"
#include "../gfx.hpp"
#include "../common.hpp"
#include "../text.hpp"
#include "../inputState.hpp"

#include <cmath>

bool IntegerBehavior::onLeftRight(Menu& menu, MenuItem& item, int dir)
{
	if ((gfx.menuCycles % scrollInterval) != 0)
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

static int filterDigits(int k)
{
	return std::isdigit(k) ? k : 0;
}

int IntegerBehavior::onEnter(Menu& menu, MenuItem& item)
{
	g_soundPlayer->play(common.soundHook[SoundMenuSelect]);

	if(!allowEntry)
		return -1; // Not allowed

	int x, y;
	if(menu.itemPosition(item, x, y))
	{
		x += menu.valueOffsetX;
		int digits = 1 + int(std::floor(std::log10(double(max))));

		int* destPtr = &v;
		int minVal = min, maxVal = max;
		bool pct = percentage;

		gfx.stateStack.push(std::make_unique<InputStringState>(
			toString(v), digits, x + 2, y, filterDigits, "", false,
			[destPtr, minVal, maxVal, pct, &menu, &item](bool accepted, std::string const& result) {
				if (accepted && !result.empty())
				{
					int val = std::atoi(result.c_str());
					if (val < minVal) val = minVal;
					else if (val > maxVal) val = maxVal;
					*destPtr = val;
				}
				// Update the menu item display
				item.value = toString(*destPtr);
				item.hasValue = true;
				if (pct)
					item.value += "%";
			}), &gfx);
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
