#pragma once

#include "menu.hpp"

struct Common;
struct ItemBehavior;

struct MainMenu : Menu
{
	enum
	{
		MaResumeGame,
		MaNewGame,
		MaSettings,
		MaPlayer1Settings,
		MaPlayer2Settings,
		MaAdvanced,
		MaQuit,
		MaReplays,
		MaReplay,
		MaTc,
		MaHostGame,
		MaJoinGame,
		MaNetPlayerSettings,
		MaHostOnline,
		MaJoinOnline,
	};

	MainMenu(int x, int y)
	: Menu(x, y)
	{
	}

	virtual ItemBehavior* getItemBehavior(Common& common, MenuItem& item);
};
