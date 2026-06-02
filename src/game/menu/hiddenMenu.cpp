#include "hiddenMenu.hpp"

#include "arrayEnumBehavior.hpp"

#include "../filesystem.hpp"
#include "../gfx.hpp"
#include "../mixer/player.hpp"

static std::string const kBotWeaponSel[3] = {"RANDOM", "PICK", "KEEP"};

ItemBehavior* HiddenMenu::GetItemBehavior(Common& common, MenuItem& item) {
  switch (item.id) {
    case kRecordReplays:
      return new BooleanSwitchBehavior(common, gfx.settings->record_replays);
    case kLoadPowerLevels:
      return new BooleanSwitchBehavior(common, gfx.settings->load_powerlevel_palette);
    case kDoubleRes:
      return new BooleanSwitchBehavior(common, gfx.double_res, [](bool v) { gfx.SetDoubleRes(v); });
    case kFullscreen:
      return new BooleanSwitchBehavior(common, gfx.settings->fullscreen,
                                       [](bool v) { gfx.SetFullscreen(v); });
    case kAiFrames:
      return new IntegerBehavior(common, gfx.settings->ai_frames, 1, 70 * 5);
    case kAiMutations:
      return new IntegerBehavior(common, gfx.settings->ai_mutations, 1, 20);
    case kPaletteSelect:
      return new IntegerBehavior(common, palette_color, 0, 255);
    case kShadows:
      return new BooleanSwitchBehavior(common, gfx.settings->shadow);
    case kScreenSync:
      return new BooleanSwitchBehavior(common, gfx.settings->screen_sync);
    case kSelectBotWeapons:
      return new ArrayEnumBehavior(common, gfx.settings->select_bot_weapons, kBotWeaponSel);
    case kAiTraces:
      return new BooleanSwitchBehavior(common, gfx.settings->ai_traces);
    case kAiParallels:
      return new IntegerBehavior(common, gfx.settings->ai_parallels, 1, 16);
    case kAllowViewingSpawnPoint:
      return new BooleanSwitchBehavior(common, gfx.settings->allow_viewing_spawn_point);
    case kSingleScreenReplay:
      return new BooleanSwitchBehavior(common, gfx.settings->single_screen_replay);
    case kSpectatorWindow:
      return new BooleanSwitchBehavior(common, gfx.settings->spectator_window, [](bool v) {
        gfx.settings->spectator_window = v;
        gfx.SetVideoMode();
      });

    default:
      return Menu::GetItemBehavior(common, item);
  }
}

void HiddenMenu::OnUpdate() {}

void HiddenMenu::DrawItemOverlay(Common& common, MenuItem& item, int x, int y, bool selected,
                                 bool disabled) {
  if (item.id == kPaletteSelect)  // Color settings
  {
    int w = 30;
    int offs_x = 44;

    DrawRoundedBox(gfx.play_renderer.bmp, x + offs_x, y, selected ? 168 : 0, 7, w);
    FillRect(gfx.play_renderer.bmp, x + offs_x + 1, y + 1, w + 1, 5, palette_color);
  }
}
