#ifndef LIERO_MENU_HIDDEN_MENU_HPP
#define LIERO_MENU_HIDDEN_MENU_HPP

#include "menu.hpp"

struct Common;
struct ItemBehavior;

struct HiddenMenu : Menu
{
	enum
	{
		Extensions,
		RecordReplays,
		Replays,
		LoadPowerLevels,
		ScalingFilter,
		FullscreenW,
		FullscreenH,
		Depth32
	};
	
	HiddenMenu(int x, int y)
	: Menu(x, y)
	{
	}
	
	virtual ItemBehavior* getItemBehavior(Common& common, int item);
};

#endif // LIERO_MENU_HIDDEN_MENU_HPP