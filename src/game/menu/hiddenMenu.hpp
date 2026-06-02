#pragma once

#include "menu.hpp"

struct Common;
struct ItemBehavior;

struct HiddenMenu : Menu {
  enum {
    kRecordReplays,
    kLoadPowerLevels,
    // ScalingFilter,
    kDoubleRes,
    kFullscreen,
    kAiFrames,
    kAiMutations,
    kPaletteSelect,
    kShadows,
    kScreenSync,
    kSelectBotWeapons,
    kAiTraces,
    kAiParallels,
    kAllowViewingSpawnPoint,
    kSingleScreenReplay,
    kSpectatorWindow,
  };

  HiddenMenu(int x, int y) : Menu(x, y), palette_color(0) {}

  virtual ItemBehavior* GetItemBehavior(Common& common, MenuItem& item);

  virtual void DrawItemOverlay(Common& common, MenuItem& item, int x, int y, bool selected,
                               bool disabled);

  virtual void OnUpdate();

  int palette_color;
};
