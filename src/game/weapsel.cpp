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

WeaponSelection::WeaponSelection(Game& game)
    : game(game),
      enabled_weaps(0),
      is_ready(game.viewports.size()),
      menus(game.viewports.size()),
      cached_background(false),
      cached_spectator_background(false),
      focused(true) {
  Common& common = *game.common;

  for (int i = 0; i < 40; ++i) {
    if (game.settings->weap_table[i] == 0) ++enabled_weaps;
  }

  for (std::size_t i = 0; i < menus.size(); ++i) {
    bool weap_used[256] = {};

    Viewport& vp = *game.viewports[i];

    Worm& worm = *game.WormByIdx(vp.worm_idx);
    WormSettings& ws = *worm.settings;

    menus[i].items.push_back(MenuItem(57, 57, LS(Randomize)));

    {
      int x = vp.rect.CenterX() - 31;
      int y = vp.rect.CenterY() - 51;
      menus[i].Place(x, y);
    }

    bool random_weapons = (ws.controller != 0 && game.settings->select_bot_weapons == 0);

    for (int j = 0; j < Settings::kSelectableWeapons; ++j) {
      if (ws.weapons[j] == 0 || random_weapons) {
        ws.weapons[j] = game.rand(1, 41);
      }

      bool enough_weapons = (enabled_weaps >= Settings::kSelectableWeapons);

      if (game.settings->weap_table[common.weap_order[ws.weapons[j] - 1]] > 0) {
        while (true) {
          ws.weapons[j] = game.rand(1, 41);

          int w = common.weap_order[ws.weapons[j] - 1];

          if ((!enough_weapons || !weap_used[w]) && game.settings->weap_table[w] <= 0) break;
        }
      }

      int w = common.weap_order[ws.weapons[j] - 1];

      weap_used[w] = true;

      WormWeapon& ww = worm.weapons[j];

      ww.ammo = 0;
      ww.type = &common.weapons[w];

      menus[i].items.push_back(MenuItem(48, 48, common.weapons[w].name));
    }

    menus[i].items.push_back(MenuItem(10, 10, LS(Done)));

    worm.current_weapon = 0;

    menus[i].MoveToFirstVisible();
    is_ready[i] = (ws.controller != 0 && game.settings->select_bot_weapons != 1);
  }
}

void WeaponSelection::DrawSpectatorViewports(Renderer& renderer, GameState state) {
  Common& common = *game.common;
  int center_x = renderer.render_res_x / 2;
  int center_y = renderer.render_res_y / 4;

  if (!cached_spectator_background) {
    if (game.settings->level_file.empty()) {
      common.font.DrawCenteredText(renderer.bmp, LS(LevelRandom), center_x, center_y - 32, 7, 2);
    } else {
      auto level_name = GetBasename(GetLeaf(gfx.settings->level_file));
      common.font.DrawCenteredText(renderer.bmp, LS(LevelIs1) + level_name + LS(LevelIs2), center_x,
                                   center_y - 32, 7, 2);
    }

    Worm& worm0 = *game.WormByIdx(0);
    Worm& worm1 = *game.WormByIdx(1);
    std::string vs_text = worm0.settings->name + " vs " + worm1.settings->name;
    // put worm color boxes on a nice spot even if no player names have been entered
    int text_size = std::max(common.font.GetDims(vs_text) * 2, 48);
    common.font.DrawCenteredText(renderer.bmp, vs_text, center_x, center_y, 7, 2);
    FillRect(renderer.bmp, center_x - (text_size / 2) - 1, center_y + 23 - 1, 16, 16, 7);
    FillRect(renderer.bmp, center_x - text_size / 2, center_y + 23, 14, 14,
             Palette::kWormSpriteColorBase[0]);
    FillRect(renderer.bmp, center_x + (text_size / 2) - 16 - 1, center_y + 23 - 1, 16, 16, 7);
    FillRect(renderer.bmp, center_x + text_size / 2 - 16, center_y + 23, 14, 14,
             Palette::kWormSpriteColorBase[1]);
    common.font.DrawCenteredText(renderer.bmp, "WEAPON SELECTION", center_x, center_y + 48, 7, 2);
    game.level.DrawMiniature(renderer.bmp, center_x - 126, renderer.render_res_y - 208, 2);

    gfx.frozen_spectator_screen.Copy(renderer.bmp);
    cached_spectator_background = true;
  }

  renderer.bmp.Copy(gfx.frozen_spectator_screen);

  if (!focused) return;

  if (!is_ready[0]) {
    menus[0].Draw(common, renderer, false, 10);
  }
  if (!is_ready[1]) {
    menus[1].Draw(common, renderer, false, 560);
  }

  // TODO: This just uses the currently activated palette, which might well be wrong.
  gfx.single_screen_renderer.pal = gfx.single_screen_renderer.origpal;
  gfx.single_screen_renderer.pal.RotateFrom(gfx.single_screen_renderer.origpal, 168, 174,
                                            gfx.menu_cycles);
  gfx.single_screen_renderer.pal.Fade(gfx.single_screen_renderer.fade_value);
}

void WeaponSelection::DrawNormalViewports(Renderer& renderer, GameState state) {
  Common& common = *game.common;

  if (!cached_background) {
    game.Draw(renderer, state, false);

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

  if (!focused) return;

  DrawRoundedBox(renderer.bmp, 114, 2, 0, 7, common.font.GetDims(LS(SelWeap)));

  common.font.DrawString(renderer.bmp, LS(SelWeap), 116, 3, 50);

  for (std::size_t i = 0; i < menus.size(); ++i) {
    Menu& weapon_menu = menus[i];

    Viewport& vp = *game.viewports[i];

    Worm& worm = *game.WormByIdx(vp.worm_idx);
    WormSettings& ws = *worm.settings;

    int width = common.font.GetDims(ws.name);
    DrawRoundedBox(renderer.bmp, weapon_menu.x + 29 - width / 2, weapon_menu.y - 11, 0, 7, width);
    common.font.DrawString(renderer.bmp, ws.name, weapon_menu.x + 31 - width / 2,
                           weapon_menu.y - 10, Palette::kWormSpriteColorBase[worm.index] + 1);

    if (!is_ready[i]) {
      menus[i].Draw(common, gfx.play_renderer, false);
    }
  }

  // TODO: This just uses the currently activated palette, which might well be wrong.
  gfx.play_renderer.pal = gfx.play_renderer.origpal;
  gfx.play_renderer.pal.RotateFrom(gfx.play_renderer.origpal, 168, 174, gfx.menu_cycles);
  gfx.play_renderer.pal.Fade(gfx.play_renderer.fade_value);
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
    int weap_id = menus[i].Selection() - 1;

    Viewport& vp = *game.viewports[i];
    Worm& worm = *game.WormByIdx(vp.worm_idx);

    WormSettings& ws = *worm.settings;

    if (!is_ready[i]) {
      // Find this player's gamepad (if using one)
      int gp_idx = -1;
      if (ws.input_device != WormSettingsExtensions::kInputKeyboard)
        gp_idx = gfx.FindGamepadForPlayer(vp.worm_idx);

      if (weap_id >= 0 && weap_id < Settings::kSelectableWeapons) {
        bool left = worm.Pressed(Worm::kLeft);
        if (!left && gp_idx >= 0 && gfx.joysticks[gp_idx].axis_button_state[1])  // LEFTX negative
          left = true;

        if (left) {
          worm.Release(Worm::kLeft);

          game.sound_player->Play(common.sound_hook[SoundMenuMoveUp]);

          do {
            --ws.weapons[weap_id];
            if (ws.weapons[weap_id] < 1) ws.weapons[weap_id] = (uint32_t)common.weapons.size();
          } while (game.settings->weap_table[common.weap_order[ws.weapons[weap_id] - 1]] != 0);

          int w = common.weap_order[ws.weapons[weap_id] - 1];
          worm.weapons[weap_id].type = &common.weapons[w];
          menus[i].Selected()->string = common.weapons[w].name;
        }

        bool right = worm.Pressed(Worm::kRight);
        if (!right && gp_idx >= 0 && gfx.joysticks[gp_idx].axis_button_state[0])  // LEFTX positive
          right = true;

        if (right) {
          worm.Release(Worm::kRight);

          game.sound_player->Play(common.sound_hook[SoundMenuMoveDown]);

          do {
            ++ws.weapons[weap_id];
            if (ws.weapons[weap_id] > (uint32_t)common.weapons.size()) ws.weapons[weap_id] = 1;
          } while (game.settings->weap_table[common.weap_order[ws.weapons[weap_id] - 1]] != 0);

          int w = common.weap_order[ws.weapons[weap_id] - 1];
          worm.weapons[weap_id].type = &common.weapons[w];
          menus[i].Selected()->string = common.weapons[w].name;
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

          bool enough_weapons = (enabled_weaps >= Settings::kSelectableWeapons);

          for (int j = 0; j < Settings::kSelectableWeapons; ++j) {
            while (true) {
              ws.weapons[j] = game.rand(1, 41);

              int w = common.weap_order[ws.weapons[j] - 1];

              if ((!enough_weapons || !weap_used[w]) && game.settings->weap_table[w] <= 0) break;
            }

            int w = common.weap_order[ws.weapons[j] - 1];

            weap_used[w] = true;

            menus[i].items[j + 1].string = common.weapons[w].name;
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
  for (std::size_t i = 0; i < game.worms.size(); ++i) {
    Worm& worm = *game.worms[i];

    worm.InitWeapons(game);
  }
  game.ReleaseControls();

  // TODO: Make sure the weapon selection is transfered back to Gfx to be saved
}

void WeaponSelection::Focus() { focused = true; }

void WeaponSelection::Unfocus() { focused = false; }
