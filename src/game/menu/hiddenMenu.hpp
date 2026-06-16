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
    kColorMode,
    kMaxSpectatorRenderHeight,
  };

  HiddenMenu(int x, int y) : Menu(x, y) {}

  ItemBehavior* GetItemBehavior(Common& common, MenuItem& item) override;

  void DrawItemOverlay(Common& common, MenuItem& item, int x, int y, bool selected,
                       bool disabled) override;

  void OnUpdate() override;

  int palette_color{0};
};
