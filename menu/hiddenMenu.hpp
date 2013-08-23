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
		Replays,
		LoadPowerLevels,
		ScalingFilter,
		DoubleRes,
		Fullscreen,
		FullscreenW,
		FullscreenH,
		AiFrames,
		AiMutations,
		PaletteSelect,
		LoadOptions,
		SaveOptions,
		Shadows,
		ScreenSync,
		LoadChange,
		SelectBotWeapons
	};
	
	HiddenMenu(int x, int y)
	: Menu(x, y)
	, paletteColor(0)
	{
	}
	
	virtual ItemBehavior* getItemBehavior(Common& common, int item);

	virtual void drawItemOverlay(Common& common, int item, int x, int y, bool selected, bool disabled);

	int paletteColor;
};

#endif // UUID_C2D646F783444E7630AA27BB8F6C0B15
