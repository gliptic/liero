
#include "hiddenMenu.hpp"

#include "arrayEnumBehavior.hpp"

#include "../gfx.hpp"
#include "../sfx.hpp"
#include "../filesystem.hpp"

struct Depth32Behavior : BooleanSwitchBehavior
{
	Depth32Behavior(Common& common, bool& v)
	: BooleanSwitchBehavior(common, v)
	{
	}
	
	int onEnter(Menu& menu, int item)
	{
		BooleanSwitchBehavior::onEnter(menu, item);
		gfx.setVideoMode();
		return -1;
	}
	
	bool onLeftRight(Menu& menu, int item, int dir)
	{
		BooleanSwitchBehavior::onLeftRight(menu, item, dir);
		gfx.setVideoMode();
		return true;
	}
};

struct OptionsSaveBehavior : ItemBehavior
{
	OptionsSaveBehavior(Common& common)
	: common(common)
	{
	}
	
	int onEnter(Menu& menu, int item)
	{
		sfx.play(common, 27);
		
		int x, y;
		if(!menu.itemPosition(item, x, y))
			return -1;
			
		x += menu.valueOffsetX + 2;
		
		std::string name = gfx.settingsFile;
		if(gfx.inputString(name, 30, x, y) && !name.empty())
		{
			gfx.saveSettings(name);
		}
				
		sfx.play(common, 27);
		
		onUpdate(menu, item);
		return -1;
	}
	
	void onUpdate(Menu& menu, int item)
	{
		menu.items[item].value = getLeaf(gfx.settingsFile);
		menu.items[item].hasValue = true;
	}
	
	Common& common;
};

struct OptionsSelectBehavior : ItemBehavior
{
	OptionsSelectBehavior(Common& common)
	: common(common)
	{
	}

	int onEnter(Menu& menu, int item)
	{
		sfx.play(common, 27);
		gfx.selectOptions();
		sfx.play(common, 27);
		menu.updateItems(common);
		return -1;
	}

	Common& common;
};

struct ReplaySelectBehavior : ItemBehavior
{
	ReplaySelectBehavior(Common& common)
	: common(common)
	{
	}

	int onEnter(Menu& menu, int item)
	{
		sfx.play(common, 27);
		int ret = gfx.selectReplay();
		sfx.play(common, 27);
		return ret;
	}

	Common& common;
};

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

ItemBehavior* HiddenMenu::getItemBehavior(Common& common, int item)
{
	switch(item)
	{
		case RecordReplays:
			return new BooleanSwitchBehavior(common, gfx.settings->recordReplays);
		case Replays:
			return new ReplaySelectBehavior(common);
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
		case LoadOptions:
			return new OptionsSelectBehavior(common);
		case SaveOptions:
			return new OptionsSaveBehavior(common);
		case Shadows:
			return new BooleanSwitchBehavior(common, gfx.settings->shadow);
		case ScreenSync:
			return new BooleanSwitchBehavior(common, gfx.settings->screenSync);
		case LoadChange:
			return new BooleanSwitchBehavior(common, gfx.settings->loadChange);
		case SelectBotWeapons:
			return new ArrayEnumBehavior(common, gfx.settings->selectBotWeapons, botWeaponSel);
		default:
			return Menu::getItemBehavior(common, item);
	}
}

void HiddenMenu::drawItemOverlay(Common& common, int item, int x, int y, bool selected, bool disabled)
{
	if(item == PaletteSelect) //Color settings
	{
		int w = 30;
		int offsX = 44;
		
		drawRoundedBox(gfx.screenBmp, x + offsX, y, selected ? 168 : 0, 7, w);
		fillRect(gfx.screenBmp, x + offsX + 1, y + 1, w + 1, 5, paletteColor);
	}
}