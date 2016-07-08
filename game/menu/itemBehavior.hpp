#ifndef UUID_8BF7504F0306489E5EA42995B2020DD2
#define UUID_8BF7504F0306489E5EA42995B2020DD2

#include <gvl/resman/shared_ptr.hpp>
#include <vector>

struct Menu;
struct MenuItem;

struct ItemBehavior
{
	ItemBehavior()
	{
	}

	virtual bool onLeftRight(Menu& menu, MenuItem& item, int dir)
	{
		return true;
	}
	
	virtual int onEnter(Menu& menu, MenuItem& item)
	{
		return -1;
	}
	
	virtual void onUpdate(Menu& menu, MenuItem& item)
	{
	}
};

#endif // UUID_8BF7504F0306489E5EA42995B2020DD2
