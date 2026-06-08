#include "weaponMenuState.hpp"

#include <utility>

#include "common.hpp"
#include "gfx.hpp"
#include "inputState.hpp"
#include "keys.hpp"
#include "menu/arrayEnumBehavior.hpp"
#include "mixer/player.hpp"
#include "settings.hpp"
#include "text.hpp"

struct WeaponMenu : Menu {
  WeaponMenu(int x, int y) : Menu(x, y) {}

  ItemBehavior* GetItemBehavior(Common& common, MenuItem& item) override {
    int const kIndex = common.weap_order[item.id];
    return new ArrayEnumBehavior(common, gfx.settings->weap_table[kIndex],
                                 common.texts.weap_states);
  }
};

WeaponMenuState::WeaponMenuState() = default;

void WeaponMenuState::Enter() {
  Common& common = *gfx->common;

  auto menu = std::make_unique<WeaponMenu>(179, 28);
  menu->SetHeight(14);
  menu->value_offset_x = 89;

  for (int i = 0; std::cmp_less(i, common.weapons.size()); ++i) {
    int const kIndex = common.weap_order[i];
    menu->AddItem(MenuItem(48, 7, common.weapons[kIndex].name, i));
  }

  menu->MoveToFirstVisible();
  menu->UpdateItems(common);

  weaponMenu_ = std::move(menu);
}

void WeaponMenuState::HandleEvent(SDL_Event& ev) { gfx->ProcessEvent(ev); }

bool WeaponMenuState::Update() {
  if (done_) {
    return false;
  }

  Common& common = *gfx->common;

  if (gfx->TestSdlKeyOnce(SDL_SCANCODE_UP) || gfx->TestControlOnce(WormSettingsExtensions::kUp) ||
      gfx->TestGamepadDirOnce(SDL_GAMEPAD_BUTTON_DPAD_UP)) {
    g_sound_player->Play(common.sound_hook[SoundMenuMoveDown]);
    weaponMenu_->Movement(-1);
  }

  if (gfx->TestSdlKeyOnce(SDL_SCANCODE_DOWN) ||
      gfx->TestControlOnce(WormSettingsExtensions::kDown) ||
      gfx->TestGamepadDirOnce(SDL_GAMEPAD_BUTTON_DPAD_DOWN)) {
    g_sound_player->Play(common.sound_hook[SoundMenuMoveUp]);
    weaponMenu_->Movement(1);
  }

  if (gfx->TestSdlKeyOnce(SDL_SCANCODE_LEFT) ||
      gfx->TestControlOnce(WormSettingsExtensions::kLeft) ||
      gfx->TestGamepadDirOnce(SDL_GAMEPAD_BUTTON_DPAD_LEFT)) {
    weaponMenu_->OnLeftRight(common, -1);
  }
  if (gfx->TestSdlKeyOnce(SDL_SCANCODE_RIGHT) ||
      gfx->TestControlOnce(WormSettingsExtensions::kRight) ||
      gfx->TestGamepadDirOnce(SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) {
    weaponMenu_->OnLeftRight(common, 1);
  }

  if (Settings::kExtensions) {
    if (gfx->TestSdlKeyOnce(SDL_SCANCODE_PAGEUP)) {
      g_sound_player->Play(common.sound_hook[SoundMenuMoveDown]);
      weaponMenu_->MovementPage(-1);
    }

    if (gfx->TestSdlKeyOnce(SDL_SCANCODE_PAGEDOWN)) {
      g_sound_player->Play(common.sound_hook[SoundMenuMoveUp]);
      weaponMenu_->MovementPage(1);
    }
  }

  weaponMenu_->OnKeys(gfx->key_buf, gfx->key_buf_ptr);

  if (gfx->TestSdlKeyOnce(SDL_SCANCODE_ESCAPE) ||
      gfx->TestControlOnce(WormSettingsExtensions::kJump) ||
      gfx->TestGamepadButtonOnce(SDL_GAMEPAD_BUTTON_EAST) ||
      gfx->TestGamepadButtonOnce(SDL_GAMEPAD_BUTTON_SOUTH)) {
    int count = 0;

    for (unsigned int const kI : gfx->settings->weap_table) {
      if (kI == 0) {
        ++count;
      }
    }

    if (count > 0) {
      done_ = true;
      return false;
    }

    gfx->state_stack.Push(std::make_unique<InfoBoxState>(LS(NoWeaps), 223, 68, false), gfx);
  }

  return true;
}

void WeaponMenuState::Draw() {
  Common& common = *gfx->common;

  gfx->play_renderer.bmp.Copy(gfx->frozen_screen);
  gfx->DrawBasicMenu();

  DrawRoundedBox(gfx->play_renderer.bmp, 179, 20, 0, 7, common.font.GetDims(LS(Weapon)));
  DrawRoundedBox(gfx->play_renderer.bmp, 249, 20, 0, 7, common.font.GetDims(LS(Availability)));

  common.font.DrawString(gfx->play_renderer.bmp, LS(Weapon), 181, 21, 50);
  common.font.DrawString(gfx->play_renderer.bmp, LS(Availability), 251, 21, 50);

  weaponMenu_->Draw(common, gfx->play_renderer, /*disabled=*/false);
}
