#include "weaponMenuState.hpp"

#include "gfx.hpp"
#include "mixer/player.hpp"
#include "text.hpp"
#include "keys.hpp"
#include "common.hpp"
#include "settings.hpp"
#include "inputState.hpp"
#include "menu/arrayEnumBehavior.hpp"

struct WeaponMenu : Menu
{
	WeaponMenu(int x, int y)
	: Menu(x, y)
	{
	}

	ItemBehavior* getItemBehavior(Common& common, MenuItem& item)
	{
		int index = common.weapOrder[item.id];
		return new ArrayEnumBehavior(common, gfx.settings->weapTable[index], common.texts.weapStates);
	}
};

WeaponMenuState::WeaponMenuState()
{
}

void WeaponMenuState::enter()
{
	Common& common = *gfx->common;

	auto menu = std::make_unique<WeaponMenu>(179, 28);
	menu->setHeight(14);
	menu->valueOffsetX = 89;

	for (int i = 0; i < (int)common.weapons.size(); ++i)
	{
		int index = common.weapOrder[i];
		menu->addItem(MenuItem(48, 7, common.weapons[index].name, i));
	}

	menu->moveToFirstVisible();
	menu->updateItems(common);

	weaponMenu_ = std::move(menu);
}

void WeaponMenuState::handleEvent(SDL_Event& ev)
{
	gfx->processEvent(ev);
}

bool WeaponMenuState::update()
{
	if (done_)
		return false;

	Common& common = *gfx->common;

	if (gfx->testSDLKeyOnce(SDL_SCANCODE_UP)
	|| gfx->testControlOnce(WormSettingsExtensions::Up)
	|| gfx->testGamepadDirOnce(SDL_GAMEPAD_BUTTON_DPAD_UP))
	{
		g_soundPlayer->play(common.soundHook[SoundMenuMoveDown]);
		weaponMenu_->movement(-1);
	}

	if (gfx->testSDLKeyOnce(SDL_SCANCODE_DOWN)
	|| gfx->testControlOnce(WormSettingsExtensions::Down)
	|| gfx->testGamepadDirOnce(SDL_GAMEPAD_BUTTON_DPAD_DOWN))
	{
		g_soundPlayer->play(common.soundHook[SoundMenuMoveUp]);
		weaponMenu_->movement(1);
	}

	if (gfx->testSDLKeyOnce(SDL_SCANCODE_LEFT)
	|| gfx->testControlOnce(WormSettingsExtensions::Left)
	|| gfx->testGamepadDirOnce(SDL_GAMEPAD_BUTTON_DPAD_LEFT))
	{
		weaponMenu_->onLeftRight(common, -1);
	}
	if (gfx->testSDLKeyOnce(SDL_SCANCODE_RIGHT)
	|| gfx->testControlOnce(WormSettingsExtensions::Right)
	|| gfx->testGamepadDirOnce(SDL_GAMEPAD_BUTTON_DPAD_RIGHT))
	{
		weaponMenu_->onLeftRight(common, 1);
	}

	if (gfx->settings->extensions)
	{
		if (gfx->testSDLKeyOnce(SDL_SCANCODE_PAGEUP))
		{
			g_soundPlayer->play(common.soundHook[SoundMenuMoveDown]);
			weaponMenu_->movementPage(-1);
		}

		if (gfx->testSDLKeyOnce(SDL_SCANCODE_PAGEDOWN))
		{
			g_soundPlayer->play(common.soundHook[SoundMenuMoveUp]);
			weaponMenu_->movementPage(1);
		}
	}

	weaponMenu_->onKeys(gfx->keyBuf, gfx->keyBufPtr);

	if (gfx->testSDLKeyOnce(SDL_SCANCODE_ESCAPE)
	|| gfx->testControlOnce(WormSettingsExtensions::Jump)
	|| gfx->testGamepadButtonOnce(SDL_GAMEPAD_BUTTON_EAST)
	|| gfx->testGamepadButtonOnce(SDL_GAMEPAD_BUTTON_SOUTH))
	{
		int count = 0;

		for (int i = 0; i < 40; ++i)
		{
			if (gfx->settings->weapTable[i] == 0)
				++count;
		}

		if (count > 0)
		{
			done_ = true;
			return false;
		}

		gfx->stateStack.push(std::make_unique<InfoBoxState>(LS(NoWeaps), 223, 68, false), gfx);
	}

	return true;
}

void WeaponMenuState::draw()
{
	Common& common = *gfx->common;

	gfx->playRenderer.bmp.copy(gfx->frozenScreen);
	gfx->drawBasicMenu();

	drawRoundedBox(gfx->playRenderer.bmp, 179, 20, 0, 7, common.font.getDims(LS(Weapon)));
	drawRoundedBox(gfx->playRenderer.bmp, 249, 20, 0, 7, common.font.getDims(LS(Availability)));

	common.font.drawText(gfx->playRenderer.bmp, LS(Weapon), 181, 21, 50);
	common.font.drawText(gfx->playRenderer.bmp, LS(Availability), 251, 21, 50);

	weaponMenu_->draw(common, gfx->playRenderer, false);
}
