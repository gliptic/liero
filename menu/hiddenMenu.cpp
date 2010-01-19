
#include "hiddenMenu.hpp"

#include "arrayEnumBehavior.hpp"

#include "../gfx.hpp"
#include "../sfx.hpp"

struct ExtensionsSwitchBehavior : BooleanSwitchBehavior
{
	ExtensionsSwitchBehavior(Common& common, bool& v)
	: BooleanSwitchBehavior(common, v)
	{
	}
	
	void onUpdate(Menu& menu, int item)
	{
		BooleanSwitchBehavior::onUpdate(menu, item);
		
		gfx.updateExtensions(v);
	}
};

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

struct ReplaySelectBehavior : ItemBehavior
{
	int onEnter(Menu& menu, int item)
	{
		sfx.play(27);
		int ret = gfx.selectReplay();
		sfx.play(27);
		return ret;
	}
};

static std::string const scaleFilterNames[Settings::SfMax] =
{
	"Nearest",
	"Scale2X"
};

ItemBehavior* HiddenMenu::getItemBehavior(Common& common, int item)
{
	switch(item)
	{
		case Extensions:
			return new ExtensionsSwitchBehavior(common, gfx.settings->extensions);
		case RecordReplays:
			return new BooleanSwitchBehavior(common, gfx.settings->recordReplays);
		case Replays:
			return new ReplaySelectBehavior();
		case LoadPowerLevels:
			return new BooleanSwitchBehavior(common, gfx.settings->loadPowerlevelPalette);
		case ScalingFilter:
			return new ArrayEnumBehavior(common, gfx.settings->scaleFilter, scaleFilterNames);
		case FullscreenW:
			return new IntegerBehavior(common, gfx.settings->fullscreenW, 0, 9999, 0);
		case FullscreenH:
			return new IntegerBehavior(common, gfx.settings->fullscreenH, 0, 9999, 0);
		case Depth32:
			return new Depth32Behavior(common, gfx.settings->depth32);
		default:
			return Menu::getItemBehavior(common, item);
	}
}
