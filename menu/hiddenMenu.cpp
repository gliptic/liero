
#include "hiddenMenu.hpp"

#include "arrayEnumBehavior.hpp"

#include "../gfx.hpp"
#include "../sfx.hpp"
#include "../filesystem.hpp"

/*
struct Depth32Behavior : BooleanSwitchBehavior
{
	Depth32Behavior(Common& common, bool& v)
	: BooleanSwitchBehavior(common, v)
	{
	}
	
	int onEnter(Menu& menu, MenuItem& item)
	{
		BooleanSwitchBehavior::onEnter(menu, item);
		gfx.setVideoMode();
		return -1;
	}
	
	bool onLeftRight(Menu& menu, MenuItem& item, int dir)
	{
		BooleanSwitchBehavior::onLeftRight(menu, item, dir);
		gfx.setVideoMode();
		return true;
	}
};*/


static std::string const scaleFilterNames[Settings::SfMax] =
{
	"Nearest",
	"Scale2X"
};

static std::string const botWeaponSel[3] =
{
	"RANDOM",
	"PICK",
	"KEEP"
};

ItemBehavior* HiddenMenu::getItemBehavior(Common& common, MenuItem& item)
{
	switch(item.id)
	{
		case RecordReplays:
			return new BooleanSwitchBehavior(common, gfx.settings->recordReplays);
		case LoadPowerLevels:
			return new BooleanSwitchBehavior(common, gfx.settings->loadPowerlevelPalette);
		case ScalingFilter:
			return new ArrayEnumBehavior(common, gfx.settings->scaleFilter, scaleFilterNames);
		case DoubleRes:
			return new BooleanSwitchBehavior(common, gfx.doubleRes, [](bool v) { gfx.setDoubleRes(v); });
		case Fullscreen:
			return new BooleanSwitchBehavior(common, gfx.fullscreen, [](bool v) { gfx.setFullscreen(v); });
		case FullscreenW:
			return new IntegerBehavior(common, gfx.settings->fullscreenW, 0, 9999, 0);
		case FullscreenH:
			return new IntegerBehavior(common, gfx.settings->fullscreenH, 0, 9999, 0);
		case AiFrames:
			return new IntegerBehavior(common, gfx.settings->aiFrames, 1, 70 * 5);
		case AiMutations:
			return new IntegerBehavior(common, gfx.settings->aiMutations, 1, 20);
		case PaletteSelect:
			return new IntegerBehavior(common, paletteColor, 0, 255);
		case Shadows:
			return new BooleanSwitchBehavior(common, gfx.settings->shadow);
		case ScreenSync:
			return new BooleanSwitchBehavior(common, gfx.settings->screenSync);
		case SelectBotWeapons:
			return new ArrayEnumBehavior(common, gfx.settings->selectBotWeapons, botWeaponSel);

		default:
			return Menu::getItemBehavior(common, item);
	}
}

void HiddenMenu::onUpdate()
{
	
}

void HiddenMenu::drawItemOverlay(Common& common, MenuItem& item, int x, int y, bool selected, bool disabled)
{
	if(item.id == PaletteSelect) //Color settings
	{
		int w = 30;
		int offsX = 44;
		
		drawRoundedBox(gfx.screenBmp, x + offsX, y, selected ? 168 : 0, 7, w);
		fillRect(gfx.screenBmp, x + offsX + 1, y + 1, w + 1, 5, paletteColor);
	}
}