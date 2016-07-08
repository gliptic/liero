#include "mainMenu.hpp"

#include "game/sfx.hpp"
#include "game/gfx.hpp"

struct ReplaySelectBehavior : ItemBehavior
{
	ReplaySelectBehavior(Common& common)
	: common(common)
	{
	}

	int onEnter(Menu& menu, MenuItem& item)
	{
		sfx.play(common, 27);
		int ret = gfx.selectReplay();
		sfx.play(common, 27);
		return ret;
	}

	Common& common;
};

struct TcSelectBehavior : ItemBehavior
{
	TcSelectBehavior(Common& common)
	: common(common)
	{
	}

	int onEnter(Menu& menu, MenuItem& item)
	{
		sfx.play(common, 27);
		auto newCommon = gfx.selectTc();
		if (newCommon)
		{
			// TODO: mixer may still be using sounds from the old common
			gfx.common.reset(newCommon.release());
			return MainMenu::MaTc;
		}
		return -1;
	}

	Common& common;
};

ItemBehavior* MainMenu::getItemBehavior(Common& common, MenuItem& item)
{
	switch(item.id)
	{
		case MaReplays:
			return new ReplaySelectBehavior(common);

		case MaTc:
			return new TcSelectBehavior(common);

		default:
			return Menu::getItemBehavior(common, item);
	}
}
