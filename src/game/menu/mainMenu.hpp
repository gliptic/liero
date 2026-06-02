#pragma once

#include "menu.hpp"

struct Common;
struct ItemBehavior;

struct MainMenu : Menu {
  enum {
    kMaResumeGame,
    kMaNewGame,
    kMaSettings,
    kMaPlayer1Settings,
    kMaPlayer2Settings,
    kMaAdvanced,
    kMaQuit,
    kMaReplays,
    kMaReplay,
    kMaTc,
    kMaHostGame,
    kMaJoinGame,
    kMaNetPlayerSettings,
    kMaHostOnline,
    kMaJoinOnline,
  };

  MainMenu(int x, int y) : Menu(x, y) {}

  virtual ItemBehavior* GetItemBehavior(Common& common, MenuItem& item);
};
