#ifndef LIERO_MENU_ITEM_BEHAVIOUR_HPP
#define LIERO_MENU_ITEM_BEHAVIOUR_HPP

struct Menu;

struct ItemBehavior
{
	virtual bool onLeftRight(Menu& menu, int item, int dir)
	{
		return true;
	}
	
	virtual int onEnter(Menu& menu, int item)
	{
		return -1;
	}
	
	virtual void onUpdate(Menu& menu, int item)
	{
	}
};

#endif // LIERO_MENU_ITEM_BEHAVIOUR_HPP
