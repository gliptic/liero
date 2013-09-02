#ifndef UUID_C2D646F783444E7630AA27BB8F6C0B15
#define UUID_C2D646F783444E7630AA27BB8F6C0B15

#include "menu.hpp"

struct Common;
struct ItemBehavior;

struct HiddenMenu : Menu
{
	enum
	{
		RecordReplays,
		LoadPowerLevels,
		ScalingFilter,
		DoubleRes,
		Fullscreen,
		FullscreenW,
		FullscreenH,
		AiFrames,
		AiMutations,
		PaletteSelect,
		Shadows,
		ScreenSync,
		SelectBotWeapons,
	};
	
	HiddenMenu(int x, int y)
	: Menu(x, y)
	, paletteColor(0)
	{
	}
	
	virtual ItemBehavior* getItemBehavior(Common& common, MenuItem& item);

	virtual void drawItemOverlay(Common& common, MenuItem& item, int x, int y, bool selected, bool disabled);

	virtual void onUpdate();

	int paletteColor;
};

#endif // UUID_C2D646F783444E7630AA27BB8F6C0B15
