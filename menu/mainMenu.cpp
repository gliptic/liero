#include "mainMenu.hpp"

#include "sfx.hpp"
#include "gfx.hpp"

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

ItemBehavior* MainMenu::getItemBehavior(Common& common, MenuItem& item)
{
	switch(item.id)
	{
		case MaReplays:
			return new ReplaySelectBehavior(common);

		default:
			return Menu::getItemBehavior(common, item);
	}
}
