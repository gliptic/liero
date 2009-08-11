#ifndef LIERO_MENU_HPP
#define LIERO_MENU_HPP

#include <cstddef>
#include <string>
#include <cstdio>
#include <vector>
#include <memory>
#include "colour.hpp"

#include <gvl/resman/shared_ptr.hpp>
#include <gvl/support/cstdint.hpp>

struct Common;

struct MenuItem
{
	MenuItem(
		PalIdx colour,
		PalIdx disColour,
		std::string string)
	: colour(colour)
	, disColour(disColour)
	, string(string)
	, visible(true)
	, hasValue(false)
	{
	}
	
	void draw(Common& common, int x, int y, bool selected, bool disabled, bool centered, int valueOffsetX);
	
	PalIdx colour;
	PalIdx disColour;
	std::string string;
	
	bool hasValue;
	std::string value;
	
	bool visible;
};

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

struct Gfx;

struct Menu
{
	void readItems(FILE* f, int length, int count, bool colourPrefix, PalIdx colour = 0, PalIdx disColour = 0);
	
	void readItem(FILE* f, int offset, PalIdx colour = 0, PalIdx disColour = 0);
	
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

struct Common;

struct BooleanSwitchBehavior : ItemBehavior
{
	BooleanSwitchBehavior(Common& common, bool& v)
	: common(common), v(v)
	{
	}
	
	bool onLeftRight(Menu& menu, int item, int dir);
	int onEnter(Menu& menu, int item);
	void onUpdate(Menu& menu, int item);
	
	Common& common;
	bool& v;
};

struct IntegerBehavior : ItemBehavior
{
	IntegerBehavior(Common& common, int& v, int min, int max, int step = 1, bool percentage = false)
	: common(common), v(v)
	, min(min), max(max), step(step)
	, percentage(percentage)
	, allowEntry(true)
	{
	}
	
	bool onLeftRight(Menu& menu, int item, int dir);
	int onEnter(Menu& menu, int item);
	void onUpdate(Menu& menu, int item);
	
	
	
	Common& common;
	int& v;
	int min, max, step;
	bool percentage;
	bool allowEntry;
};

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

struct TimeBehavior : IntegerBehavior
{
	TimeBehavior(Common& common, int& v, int min, int max, int step = 1, bool percentage = false)
	: IntegerBehavior(common, v, min, max, step, percentage)
	{
	}
	
	void onUpdate(Menu& menu, int item);
};



#endif // LIERO_MENU_HPP
