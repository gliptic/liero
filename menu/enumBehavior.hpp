#ifndef UUID_171F520E38DF4A036E1AA3901EEA4801
#define UUID_171F520E38DF4A036E1AA3901EEA4801

#include "itemBehavior.hpp"

#include <gvl/support/cstdint.hpp>

struct Common;
struct Menu;

struct EnumBehavior : ItemBehavior
{
	EnumBehavior(Common& common, uint32_t& v, uint32_t min, uint32_t max, bool brokenLeftRight = false)
	: common(common), v(v)
	, min(min), max(max)
	, brokenLeftRight(brokenLeftRight)
	{
	}
	
	bool onLeftRight(Menu& menu, int item, int dir);
	int onEnter(Menu& menu, int item);
	void onUpdate(Menu& menu, int item);
	
	void change(Menu& menu, int item, int dir);
	
	Common& common;
	uint32_t& v;
	uint32_t min, max;
	bool brokenLeftRight;
};

#endif // UUID_171F520E38DF4A036E1AA3901EEA4801
