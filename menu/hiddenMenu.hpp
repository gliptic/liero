#ifndef UUID_C2D646F783444E7630AA27BB8F6C0B15
#define UUID_C2D646F783444E7630AA27BB8F6C0B15

#include "menu.hpp"

struct Common;
struct ItemBehavior;

struct HiddenMenu : Menu
{
	enum
	{
		Extensions,
		RecordReplays,
		Replays,
		LoadPowerLevels,
		ScalingFilter,
		FullscreenW,
		FullscreenH,
		Depth32
	};
	
	HiddenMenu(int x, int y)
	: Menu(x, y)
	{
	}
	
	virtual ItemBehavior* getItemBehavior(Common& common, int item);
};

#endif // UUID_C2D646F783444E7630AA27BB8F6C0B15
