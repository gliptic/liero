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
      enabledWeaps(0),
      isReady(game.viewports.size()),
      menus(game.viewports.size()),
      cachedBackground(false),
      cachedSpectatorBackground(false),
      focused(true) {
  Common& common = *game.common;

  for (int i = 0; i < 40; ++i) {
    if (game.settings->weapTable[i] == 0) ++enabledWeaps;
  }

  for (std::size_t i = 0; i < menus.size(); ++i) {
    bool weapUsed[256] = {};

    Viewport& vp = *game.viewports[i];

    Worm& worm = *game.wormByIdx(vp.wormIdx);
    WormSettings& ws = *worm.settings;

    menus[i].items.push_back(MenuItem(57, 57, LS(Randomize)));

    {
      int x = vp.rect.center_x() - 31;
      int y = vp.rect.center_y() - 51;
      menus[i].place(x, y);
    }

    bool randomWeapons = (ws.controller != 0 && game.settings->selectBotWeapons == 0);

    for (int j = 0; j < Settings::selectableWeapons; ++j) {
      if (ws.weapons[j] == 0 || randomWeapons) {
        ws.weapons[j] = game.rand(1, 41);
      }

      bool enoughWeapons = (enabledWeaps >= Settings::selectableWeapons);

      if (game.settings->weapTable[common.weapOrder[ws.weapons[j] - 1]] > 0) {
        while (true) {
          ws.weapons[j] = game.rand(1, 41);

          int w = common.weapOrder[ws.weapons[j] - 1];

          if ((!enoughWeapons || !weapUsed[w]) && game.settings->weapTable[w] <= 0) break;
        }
      }

      int w = common.weapOrder[ws.weapons[j] - 1];

      weapUsed[w] = true;

      WormWeapon& ww = worm.weapons[j];

      ww.ammo = 0;
      ww.type = &common.weapons[w];

      menus[i].items.push_back(MenuItem(48, 48, common.weapons[w].name));
    }

    menus[i].items.push_back(MenuItem(10, 10, LS(Done)));

    worm.currentWeapon = 0;

    menus[i].moveToFirstVisible();
    isReady[i] = (ws.controller != 0 && game.settings->selectBotWeapons != 1);
  }
}

void WeaponSelection::drawSpectatorViewports(Renderer& renderer, GameState state) {
  Common& common = *game.common;
  int centerX = renderer.renderResX / 2;
  int centerY = renderer.renderResY / 4;

  if (!cachedSpectatorBackground) {
    if (game.settings->levelFile.empty()) {
      common.font.drawCenteredText(renderer.bmp, LS(LevelRandom), centerX, centerY - 32, 7, 2);
    } else {
      auto levelName = getBasename(getLeaf(gfx.settings->levelFile));
      common.font.drawCenteredText(renderer.bmp, LS(LevelIs1) + levelName + LS(LevelIs2), centerX,
                                   centerY - 32, 7, 2);
    }

    Worm& worm0 = *game.wormByIdx(0);
    Worm& worm1 = *game.wormByIdx(1);
    std::string vsText = worm0.settings->name + " vs " + worm1.settings->name;
    // put worm color boxes on a nice spot even if no player names have been entered
    int textSize = std::max(common.font.getDims(vsText) * 2, 48);
    common.font.drawCenteredText(renderer.bmp, vsText, centerX, centerY, 7, 2);
    fillRect(renderer.bmp, centerX - (textSize / 2) - 1, centerY + 23 - 1, 16, 16, 7);
    fillRect(renderer.bmp, centerX - textSize / 2, centerY + 23, 14, 14,
             Palette::wormSpriteColorBase[0]);
    fillRect(renderer.bmp, centerX + (textSize / 2) - 16 - 1, centerY + 23 - 1, 16, 16, 7);
    fillRect(renderer.bmp, centerX + textSize / 2 - 16, centerY + 23, 14, 14,
             Palette::wormSpriteColorBase[1]);
    common.font.drawCenteredText(renderer.bmp, "WEAPON SELECTION", centerX, centerY + 48, 7, 2);
    game.level.drawMiniature(renderer.bmp, centerX - 126, renderer.renderResY - 208, 2);

    gfx.frozenSpectatorScreen.copy(renderer.bmp);
    cachedSpectatorBackground = true;
  }

  renderer.bmp.copy(gfx.frozenSpectatorScreen);

  if (!focused) return;

  if (!isReady[0]) {
    menus[0].draw(common, renderer, false, 10);
  }
  if (!isReady[1]) {
    menus[1].draw(common, renderer, false, 560);
  }

  // TODO: This just uses the currently activated palette, which might well be wrong.
  gfx.singleScreenRenderer.pal = gfx.singleScreenRenderer.origpal;
  gfx.singleScreenRenderer.pal.rotateFrom(gfx.singleScreenRenderer.origpal, 168, 174,
                                          gfx.menuCycles);
  gfx.singleScreenRenderer.pal.fade(gfx.singleScreenRenderer.fadeValue);
}

void WeaponSelection::drawNormalViewports(Renderer& renderer, GameState state) {
  Common& common = *game.common;

  if (!cachedBackground) {
    game.draw(renderer, state, false);

    if (game.settings->levelFile.empty()) {
      common.font.drawText(renderer.bmp, LS(LevelRandom), 0, 162, 50);
    } else {
      auto levelName = getBasename(getLeaf(gfx.settings->levelFile));
      common.font.drawText(renderer.bmp, (LS(LevelIs1) + levelName + LS(LevelIs2)), 0, 162, 50);
    }

    gfx.frozenScreen.copy(renderer.bmp);
    cachedBackground = true;
  }

  renderer.bmp.copy(gfx.frozenScreen);

  if (!focused) return;

  drawRoundedBox(renderer.bmp, 114, 2, 0, 7, common.font.getDims(LS(SelWeap)));

  common.font.drawText(renderer.bmp, LS(SelWeap), 116, 3, 50);

  for (std::size_t i = 0; i < menus.size(); ++i) {
    Menu& weaponMenu = menus[i];

    Viewport& vp = *game.viewports[i];

    Worm& worm = *game.wormByIdx(vp.wormIdx);
    WormSettings& ws = *worm.settings;

    int width = common.font.getDims(ws.name);
    drawRoundedBox(renderer.bmp, weaponMenu.x + 29 - width / 2, weaponMenu.y - 11, 0, 7, width);
    common.font.drawText(renderer.bmp, ws.name, weaponMenu.x + 31 - width / 2, weaponMenu.y - 10,
                         Palette::wormSpriteColorBase[worm.index] + 1);

    if (!isReady[i]) {
      menus[i].draw(common, gfx.playRenderer, false);
    }
  }

  // TODO: This just uses the currently activated palette, which might well be wrong.
  gfx.playRenderer.pal = gfx.playRenderer.origpal;
  gfx.playRenderer.pal.rotateFrom(gfx.playRenderer.origpal, 168, 174, gfx.menuCycles);
  gfx.playRenderer.pal.fade(gfx.playRenderer.fadeValue);
}

void WeaponSelection::draw(Renderer& renderer, GameState state, bool useSpectatorViewports) {
  if (useSpectatorViewports) {
    drawSpectatorViewports(renderer, state);
  } else {
    drawNormalViewports(renderer, state);
  }
}

bool WeaponSelection::processFrame() {
  Common& common = *game.common;

  bool allReady = true;

  for (std::size_t i = 0; i < menus.size(); ++i) {
    int weapID = menus[i].selection() - 1;

    Viewport& vp = *game.viewports[i];
    Worm& worm = *game.wormByIdx(vp.wormIdx);

    WormSettings& ws = *worm.settings;

    if (!isReady[i]) {
      // Find this player's gamepad (if using one)
      int gpIdx = -1;
      if (ws.inputDevice != WormSettingsExtensions::InputKeyboard)
        gpIdx = gfx.findGamepadForPlayer(vp.wormIdx);

      if (weapID >= 0 && weapID < Settings::selectableWeapons) {
        bool left = worm.pressed(Worm::Left);
        if (!left && gpIdx >= 0 && gfx.joysticks[gpIdx].axisButtonState[1])  // LEFTX negative
          left = true;

        if (left) {
          worm.release(Worm::Left);

          game.soundPlayer->play(common.soundHook[SoundMenuMoveUp]);

          do {
            --ws.weapons[weapID];
            if (ws.weapons[weapID] < 1) ws.weapons[weapID] = (uint32_t)common.weapons.size();
          } while (game.settings->weapTable[common.weapOrder[ws.weapons[weapID] - 1]] != 0);

          int w = common.weapOrder[ws.weapons[weapID] - 1];
          worm.weapons[weapID].type = &common.weapons[w];
          menus[i].selected()->string = common.weapons[w].name;
        }

        bool right = worm.pressed(Worm::Right);
        if (!right && gpIdx >= 0 && gfx.joysticks[gpIdx].axisButtonState[0])  // LEFTX positive
          right = true;

        if (right) {
          worm.release(Worm::Right);

          game.soundPlayer->play(common.soundHook[SoundMenuMoveDown]);

          do {
            ++ws.weapons[weapID];
            if (ws.weapons[weapID] > (uint32_t)common.weapons.size()) ws.weapons[weapID] = 1;
          } while (game.settings->weapTable[common.weapOrder[ws.weapons[weapID] - 1]] != 0);

          int w = common.weapOrder[ws.weapons[weapID] - 1];
          worm.weapons[weapID].type = &common.weapons[w];
          menus[i].selected()->string = common.weapons[w].name;
        }
      }

      bool up = worm.pressedOnce(Worm::Up);
      if (!up && gpIdx >= 0 && gfx.joysticks[gpIdx].axisPressed[3])  // LEFTY negative
      {
        gfx.joysticks[gpIdx].axisPressed[3] = false;
        up = true;
      }
      if (up) {
        game.soundPlayer->play(common.soundHook[SoundMenuMoveDown]);
        menus[i].movement(-1);
      }

      bool down = worm.pressedOnce(Worm::Down);
      if (!down && gpIdx >= 0 && gfx.joysticks[gpIdx].axisPressed[2])  // LEFTY positive
      {
        gfx.joysticks[gpIdx].axisPressed[2] = false;
        down = true;
      }
      if (down) {
        game.soundPlayer->play(common.soundHook[SoundMenuMoveUp]);
        menus[i].movement(1);
      }

      // Check Fire control OR A button on assigned gamepad
      bool confirm = worm.pressed(Worm::Fire);
      if (!confirm && gpIdx >= 0 && gfx.joysticks[gpIdx].btnPressed[SDL_GAMEPAD_BUTTON_SOUTH]) {
        gfx.joysticks[gpIdx].btnPressed[SDL_GAMEPAD_BUTTON_SOUTH] = false;
        confirm = true;
      }

      if (confirm) {
        if (menus[i].selection() == 0) {
          bool weapUsed[256] = {};

          bool enoughWeapons = (enabledWeaps >= Settings::selectableWeapons);

          for (int j = 0; j < Settings::selectableWeapons; ++j) {
            while (true) {
              ws.weapons[j] = game.rand(1, 41);

              int w = common.weapOrder[ws.weapons[j] - 1];

              if ((!enoughWeapons || !weapUsed[w]) && game.settings->weapTable[w] <= 0) break;
            }

            int w = common.weapOrder[ws.weapons[j] - 1];

            weapUsed[w] = true;

            menus[i].items[j + 1].string = common.weapons[w].name;
          }
        } else if (menus[i].selection() == 6)  // TODO: Unhardcode
        {
          game.soundPlayer->play(common.soundHook[SoundMenuSelect]);
          isReady[i] = true;
        }
      }
    }

    allReady = allReady && isReady[i];
  }

  return allReady;
}

void WeaponSelection::finalize() {
  for (std::size_t i = 0; i < game.worms.size(); ++i) {
    Worm& worm = *game.worms[i];

    worm.initWeapons(game);
  }
  game.releaseControls();

  // TODO: Make sure the weapon selection is transfered back to Gfx to be saved
}

void WeaponSelection::focus() { focused = true; }

void WeaponSelection::unfocus() { focused = false; }
