#ifndef UUID_3DC24B15AD67494EEAB541B4AE253D0F
#define UUID_3DC24B15AD67494EEAB541B4AE253D0F

#include <cstddef>
#include <string>
#include <cstdio>
#include <vector>
#include <memory>
#include "../gfx/color.hpp"

#include <gvl/resman/shared_ptr.hpp>
#include <gvl/support/cstdint.hpp>

#include "menuItem.hpp"
#include "itemBehavior.hpp"

#include "integerBehavior.hpp"
#include "booleanSwitchBehavior.hpp"
#include "timeBehavior.hpp"
#include "enumBehavior.hpp"

struct Common;

struct Gfx;

struct Menu
{
	void readItems(FILE* f, int length, int count, bool colorPrefix, PalIdx color = 0, PalIdx disColour = 0);
	
	void readItem(FILE* f, int offset, PalIdx color = 0, PalIdx disColour = 0);
	
	Menu(bool centered = false)
	{
		init(centered);
	}
	
	Menu(int x, int y, bool centered = false)
	{
		init(centered);
		place(x, y);
	}
	
	void init(bool centeredInit = false)
	{
		itemHeight = 8;
		centered = centeredInit;
		selection_ = 0;
		valueOffsetX = 0;
		x = 0;
		y = 0;
		height = 15;
		topItem = 0;
		bottomItem = 0;
		//showScroll = false;
		visibleItemCount = 0;
	}
	
	void draw(Common& common/*, int x, int y*/, bool disabled);
	
	virtual void drawItemOverlay(Common& common, int item, int x, int y, bool selected, bool disabled)
	{
		// Nothing by default
	}
	
	virtual ItemBehavior* getItemBehavior(Common& common, int item)
	{
		// Dummy item behavior by default
		return new ItemBehavior;
	}
	
	bool onLeftRight(Common& common, int dir)
	{
		std::auto_ptr<ItemBehavior> b(getItemBehavior(common, selection()));
		return b->onLeftRight(*this, selection(), dir);
	}
	
	int onEnter(Common& common)
	{
		std::auto_ptr<ItemBehavior> b(getItemBehavior(common, selection()));
		return b->onEnter(*this, selection());
	}
	
	void updateItems(Common& common)
	{
		for(std::size_t i = 0; i < items.size(); ++i)
		{
			std::auto_ptr<ItemBehavior> b(getItemBehavior(common, i));
			
			b->onUpdate(*this, i);
		}
	}
	
	void place(int newX, int newY)
	{
		x = newX;
		y = newY;
	}
	
	bool isSelectionValid()
	{
		return selection_ >= 0 && selection_ < (int)items.size();
	}
	
	void moveToFirstVisible();
	void movement(int direction);
	void movementPage(int direction);
	
	int addItem(MenuItem item);
	int addItem(MenuItem item, int pos);
	
	bool itemPosition(int item, int& x, int& y);
	
	int visibleItemIndex(int item);
	int itemFromVisibleIndex(int idx);
	
	void setHeight(int newHeight)
	{
		height = newHeight;
		setTop(topItem);
	}
	
	int selection()
	{
		return selection_;
	}
	
	void setVisibility(int item, bool state);
	int firstVisibleFrom(int item);
	int lastVisibleFrom(int item);
	void moveTo(int newSelection);
	bool isInView(int item);
	void ensureInView(int item);
	void setBottom(int newBottomVisIdx);
	void setTop(int newTopVisIdx);
	void scroll(int amount);
		
	std::vector<MenuItem> items;
	int itemHeight;
	int valueOffsetX;
	
	int x, y;
	int height;
	
	int topItem; // Visible index
	int bottomItem; // Visible index
	//bool showScroll;
	
	int visibleItemCount;
	
	bool centered;
private:
	int selection_; // Global index
};

#endif // UUID_3DC24B15AD67494EEAB541B4AE253D0F
