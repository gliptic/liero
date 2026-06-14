#include <SDL3/SDL.h>

#include "filesystem.hpp"
#include "game.hpp"
#include "gfx.hpp"
#include "gfx/renderer.hpp"
#include "menu/menu.hpp"
#include "mixer/player.hpp"
#include "text.hpp"
#include "viewport.hpp"
#include "weapsel.hpp"
#include "worm.hpp"

namespace {

// Weapon selection runs its own menu-style palette: water rotation from the
// menu clock plus the controller fade. Rebuilt before drawing each frame
// (blits resolve through pal32 at draw time).
// TODO: This just uses the currently activated palette, which might well be wrong.
void UpdateWeapselPalette(Renderer& renderer) {
  renderer.pal = renderer.Origpal();
  renderer.pal.RotateFrom(renderer.Origpal(), 168, 174, gfx.menu_cycles);
  renderer.UpdatePal32();
}

}  // namespace

WeaponSelection::WeaponSelection(Game& game)
    : game(game),

      is_ready(game.viewports.size()),
      menus(game.viewports.size()) {
  Common& common = *game.common;

  for (unsigned int const kI : game.settings->weap_table) {
    if (kI == 0) {
      ++enabled_weaps;
    }
  }

  for (std::size_t i = 0; i < menus.size(); ++i) {
    bool weap_used[256] = {};

    Viewport const& vp = *game.viewports[i];

    Worm& worm = *game.WormByIdx(vp.worm_idx);
    WormSettings& ws = *worm.settings;

    menus[i].items.emplace_back(57, 57, LS(Randomize));

    {
      int const kX = vp.rect.CenterX() - 31;
      int const kY = vp.rect.CenterY() - 51;
      menus[i].Place(kX, kY);
    }

    bool const kRandomWeapons = (ws.controller != 0 && game.settings->select_bot_weapons == 0);

    for (int j = 0; j < Settings::kSelectableWeapons; ++j) {
      if (ws.weapons[j] == 0 || kRandomWeapons) {
        ws.weapons[j] = game.rand(1, 41);
      }

      bool const kEnoughWeapons = (enabled_weaps >= Settings::kSelectableWeapons);

      if (game.settings->weap_table[common.weap_order[ws.weapons[j] - 1]] > 0) {
        while (true) {
          ws.weapons[j] = game.rand(1, 41);

          int const kW = common.weap_order[ws.weapons[j] - 1];

          if ((!kEnoughWeapons || !weap_used[kW]) && game.settings->weap_table[kW] <= 0) {
            break;
          }
        }
      }

      int const kW = common.weap_order[ws.weapons[j] - 1];

      weap_used[kW] = true;

      WormWeapon& ww = worm.weapons[j];

      ww.ammo = 0;
      ww.type = &common.weapons[kW];

      menus[i].items.emplace_back(48, 48, common.weapons[kW].name);
    }

    menus[i].items.emplace_back(10, 10, LS(Done));

    worm.current_weapon = 0;

    menus[i].MoveToFirstVisible();
    is_ready[i] = (ws.controller != 0 && game.settings->select_bot_weapons != 1);
  }
}

void WeaponSelection::DrawSpectatorViewports(Renderer& renderer, GameState /*state*/) {
  Common& common = *game.common;
  int const kCenterX = renderer.render_res_x / 2;
  int const kCenterY = renderer.render_res_y / 4;

  UpdateWeapselPalette(gfx.single_screen_renderer);

  if (!cached_spectator_background) {
    if (game.settings->level_file.empty()) {
      common.font.DrawCenteredText(renderer.bmp, LS(LevelRandom), kCenterX, kCenterY - 32, 7, 2);
    } else {
      auto level_name = GetBasename(GetLeaf(gfx.settings->level_file));
      common.font.DrawCenteredText(renderer.bmp, LS(LevelIs1) + level_name + LS(LevelIs2), kCenterX,
                                   kCenterY - 32, 7, 2);
    }

    Worm const& worm0 = *game.WormByIdx(0);
    Worm const& worm1 = *game.WormByIdx(1);
    std::string const kVsText = worm0.settings->name + " vs " + worm1.settings->name;
    // put worm color boxes on a nice spot even if no player names have been entered
    int const kTextSize = std::max(common.font.GetDims(kVsText) * 2, 48);
    common.font.DrawCenteredText(renderer.bmp, kVsText, kCenterX, kCenterY, 7, 2);
    FillRect(renderer.bmp, kCenterX - (kTextSize / 2) - 1, kCenterY + 23 - 1, 16, 16, 7);
    FillRect(renderer.bmp, kCenterX - kTextSize / 2, kCenterY + 23, 14, 14,
             Palette::kWormColorBlocks[0].base);
    FillRect(renderer.bmp, kCenterX + (kTextSize / 2) - 16 - 1, kCenterY + 23 - 1, 16, 16, 7);
    FillRect(renderer.bmp, kCenterX + kTextSize / 2 - 16, kCenterY + 23, 14, 14,
             Palette::kWormColorBlocks[1].base);
    common.font.DrawCenteredText(renderer.bmp, "WEAPON SELECTION", kCenterX, kCenterY + 48, 7, 2);
    // Fit the minimap into kSpecMinimapW×kSpecMinimapH pixels regardless of map size.
    int const kMinimapStepX =
        std::max((game.level.width + Level::kSpecMinimapW - 1) / Level::kSpecMinimapW, 1);
    int const kMinimapStepY =
        std::max((game.level.height + Level::kSpecMinimapH - 1) / Level::kSpecMinimapH, 1);
    FillRect(renderer.bmp, kCenterX - Level::kSpecMinimapW / 2, renderer.render_res_y - 208,
             Level::kSpecMinimapW, Level::kSpecMinimapH, 0);
    game.level.DrawMiniature(renderer.bmp, kCenterX - Level::kSpecMinimapW / 2,
                             renderer.render_res_y - 208, kMinimapStepX, kMinimapStepY);

    gfx.frozen_spectator_screen.Copy(renderer.bmp);
    cached_spectator_background = true;
  }

  Fill(renderer.bmp, 0);
  if (gfx.frozen_spectator_screen.pixels != nullptr) {
    BlitBitmap(renderer.bmp, gfx.frozen_spectator_screen, 0, 0, gfx.frozen_spectator_screen.w,
               gfx.frozen_spectator_screen.h);
  }

  if (!focused) {
    return;
  }

  if (!is_ready[0]) {
    menus[0].Draw(common, renderer, /*disabled=*/false, 10);
  }
  if (!is_ready[1]) {
    menus[1].Draw(common, renderer, /*disabled=*/false, 560);
  }
}

void WeaponSelection::DrawNormalViewports(Renderer& renderer, GameState state) {
  Common& common = *game.common;

  UpdateWeapselPalette(gfx.play_renderer);

  if (!cached_background) {
    game.Draw(renderer, state, /*use_spectator_viewports=*/false);
    // game.Draw rebuilds the palette for the level draw; restore the
    // weapon-selection palette for everything drawn after it.
    UpdateWeapselPalette(gfx.play_renderer);

    if (game.settings->level_file.empty()) {
      common.font.DrawString(renderer.bmp, LS(LevelRandom), 0, 162, 50);
    } else {
      auto level_name = GetBasename(GetLeaf(gfx.settings->level_file));
      common.font.DrawString(renderer.bmp, (LS(LevelIs1) + level_name + LS(LevelIs2)), 0, 162, 50);
    }

    gfx.frozen_screen.Copy(renderer.bmp);
    cached_background = true;
  }

  renderer.bmp.Copy(gfx.frozen_screen);

  if (!focused) {
    return;
  }

  DrawRoundedBox(renderer.bmp, 114, 2, 0, 7, common.font.GetDims(LS(SelWeap)));

  common.font.DrawString(renderer.bmp, LS(SelWeap), 116, 3, 50);

  for (std::size_t i = 0; i < menus.size(); ++i) {
    Menu const& weapon_menu = menus[i];

    Viewport const& vp = *game.viewports[i];

    Worm const& worm = *game.WormByIdx(vp.worm_idx);
    WormSettings const& ws = *worm.settings;

    int const kWidth = common.font.GetDims(ws.name);
    DrawRoundedBox(renderer.bmp, weapon_menu.x + 29 - kWidth / 2, weapon_menu.y - 11, 0, 7, kWidth);
    common.font.DrawString(renderer.bmp, ws.name, weapon_menu.x + 31 - kWidth / 2,
                           weapon_menu.y - 10, Palette::kWormColorBlocks[worm.index].base + 1);

    if (!is_ready[i]) {
      menus[i].Draw(common, gfx.play_renderer, /*disabled=*/false);
    }
  }
}

void WeaponSelection::Draw(Renderer& renderer, GameState state, bool use_spectator_viewports) {
  if (use_spectator_viewports) {
    DrawSpectatorViewports(renderer, state);
  } else {
    DrawNormalViewports(renderer, state);
  }
}

bool WeaponSelection::ProcessFrame() {
  Common& common = *game.common;

  bool all_ready = true;

  for (std::size_t i = 0; i < menus.size(); ++i) {
    int const kWeapId = menus[i].Selection() - 1;

    Viewport const& vp = *game.viewports[i];
    Worm& worm = *game.WormByIdx(vp.worm_idx);

    WormSettings& ws = *worm.settings;

    if (!is_ready[i]) {
      // Find this player's gamepad (if using one)
      int gp_idx = -1;
      if (ws.input_device != WormSettingsExtensions::kInputKeyboard) {
        gp_idx = gfx.FindGamepadForPlayer(vp.worm_idx);
      }

      if (kWeapId >= 0 && kWeapId < Settings::kSelectableWeapons) {
        bool left = worm.Pressed(Worm::kLeft);
        if (!left && gp_idx >= 0 && gfx.joysticks[gp_idx].axis_button_state[1]) {  // LEFTX negative
          left = true;
        }

        if (left) {
          worm.Release(Worm::kLeft);

          game.sound_player->Play(common.sound_hook[SoundMenuMoveUp]);

          do {
            --ws.weapons[kWeapId];
            if (ws.weapons[kWeapId] < 1) {
              ws.weapons[kWeapId] = static_cast<uint32_t>(common.weapons.size());
            }
          } while (game.settings->weap_table[common.weap_order[ws.weapons[kWeapId] - 1]] != 0);

          int const kW = common.weap_order[ws.weapons[kWeapId] - 1];
          worm.weapons[kWeapId].type = &common.weapons[kW];
          menus[i].Selected()->string = common.weapons[kW].name;
        }

        bool right = worm.Pressed(Worm::kRight);
        if (!right && gp_idx >= 0 &&
            gfx.joysticks[gp_idx].axis_button_state[0]) {  // LEFTX positive
          right = true;
        }

        if (right) {
          worm.Release(Worm::kRight);

          game.sound_player->Play(common.sound_hook[SoundMenuMoveDown]);

          do {
            ++ws.weapons[kWeapId];
            if (ws.weapons[kWeapId] > static_cast<uint32_t>(common.weapons.size())) {
              ws.weapons[kWeapId] = 1;
            }
          } while (game.settings->weap_table[common.weap_order[ws.weapons[kWeapId] - 1]] != 0);

          int const kW = common.weap_order[ws.weapons[kWeapId] - 1];
          worm.weapons[kWeapId].type = &common.weapons[kW];
          menus[i].Selected()->string = common.weapons[kW].name;
        }
      }

      bool up = worm.PressedOnce(Worm::kUp);
      if (!up && gp_idx >= 0 && gfx.joysticks[gp_idx].axis_pressed[3])  // LEFTY negative
      {
        gfx.joysticks[gp_idx].axis_pressed[3] = false;
        up = true;
      }
      if (up) {
        game.sound_player->Play(common.sound_hook[SoundMenuMoveDown]);
        menus[i].Movement(-1);
      }

      bool down = worm.PressedOnce(Worm::kDown);
      if (!down && gp_idx >= 0 && gfx.joysticks[gp_idx].axis_pressed[2])  // LEFTY positive
      {
        gfx.joysticks[gp_idx].axis_pressed[2] = false;
        down = true;
      }
      if (down) {
        game.sound_player->Play(common.sound_hook[SoundMenuMoveUp]);
        menus[i].Movement(1);
      }

      // Check Fire control OR A button on assigned gamepad
      bool confirm = worm.Pressed(Worm::kFire);
      if (!confirm && gp_idx >= 0 && gfx.joysticks[gp_idx].btn_pressed[SDL_GAMEPAD_BUTTON_SOUTH]) {
        gfx.joysticks[gp_idx].btn_pressed[SDL_GAMEPAD_BUTTON_SOUTH] = false;
        confirm = true;
      }

      if (confirm) {
        if (menus[i].Selection() == 0) {
          bool weap_used[256] = {};

          bool const kEnoughWeapons = (enabled_weaps >= Settings::kSelectableWeapons);

          for (int j = 0; j < Settings::kSelectableWeapons; ++j) {
            while (true) {
              ws.weapons[j] = game.rand(1, 41);

              int const kW = common.weap_order[ws.weapons[j] - 1];

              if ((!kEnoughWeapons || !weap_used[kW]) && game.settings->weap_table[kW] <= 0) {
                break;
              }
            }

            int const kW = common.weap_order[ws.weapons[j] - 1];

            weap_used[kW] = true;

            menus[i].items[j + 1].string = common.weapons[kW].name;
          }
        } else if (menus[i].Selection() == 6)  // TODO: Unhardcode
        {
          game.sound_player->Play(common.sound_hook[SoundMenuSelect]);
          is_ready[i] = true;
        }
      }
    }

    all_ready = all_ready && is_ready[i];
  }

  return all_ready;
}

void WeaponSelection::Finalize() {
  for (auto& i : game.worms) {
    Worm& worm = *i;

    worm.InitWeapons(game);
  }
  game.ReleaseControls();

  // TODO: Make sure the weapon selection is transfered back to Gfx to be saved
}

void WeaponSelection::Focus() { focused = true; }

void WeaponSelection::Unfocus() { focused = false; }
