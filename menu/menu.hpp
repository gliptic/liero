#ifndef UUID_3DC24B15AD67494EEAB541B4AE253D0F
#define UUID_3DC24B15AD67494EEAB541B4AE253D0F

#include <SDL/SDL.h>
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
struct ReaderFile;
struct Gfx;

struct Menu
{
	void readItems(ReaderFile& f, int length, int count, bool colorPrefix, PalIdx color = 0, PalIdx disColour = 0);
	
	void readItem(ReaderFile& f, int offset, PalIdx color = 0, PalIdx disColour = 0);
	
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
		searchTime = 0;
	}
	
	void draw(Common& common, bool disabled, int x = -1, bool showDisabledSelection = false);
	void process();
	
	virtual void drawItemOverlay(Common& common, MenuItem& item, int x, int y, bool selected, bool disabled)
	{
		// Nothing by default
	}
	
	virtual ItemBehavior* getItemBehavior(Common& common, MenuItem& item)
	{
		// Dummy item behavior by default
		return new ItemBehavior;
	}

	virtual void onUpdate()
	{
		// Nothing by default
	}
	
	bool onLeftRight(Common& common, int dir)
	{
		auto* s = selected();
		if (!s) return false;
		std::auto_ptr<ItemBehavior> b(getItemBehavior(common, *s));
		return b->onLeftRight(*this, *s, dir);
	}
	
	int onEnter(Common& common)
	{
		auto* s = selected();
		if (!s) return false;
		std::auto_ptr<ItemBehavior> b(getItemBehavior(common, *s));
		return b->onEnter(*this, *s);
	}

	void onKeys(SDL_keysym* begin, SDL_keysym* end, bool contains = false);
	
	void updateItems(Common& common)
	{
		for(std::size_t i = 0; i < items.size(); ++i)
		{
			std::auto_ptr<ItemBehavior> b(getItemBehavior(common, items[i]));
			
			b->onUpdate(*this, items[i]);
		}

		onUpdate();
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
	void clear();
	
	bool itemPosition(MenuItem& item, int& x, int& y);
	
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

	MenuItem* selected()
	{
		if (!isSelectionValid())
			return 0;
		return &items[selection_];
	}

	int selectedId()
	{
		MenuItem* s = selected();
		return s ? s->id : -1;
	}

	int indexFromId(int id)
	{
		for (int i = 0; i < (int)items.size(); ++i)
		{
			if (items[i].id == id)
				return i;
		}

		return -1;
	}

	MenuItem* itemFromId(int id)
	{
		for (int i = 0; i < (int)items.size(); ++i)
		{
			if (items[i].id == id)
				return &items[i];
		}

		return 0;
	}
	
	void setVisibility(int id, bool state);
	int firstVisibleFrom(int item);
	int lastVisibleFrom(int item);
	void moveTo(int newSelection);
	void moveToId(int id);
	bool isInView(int item);
	void ensureInView(int item);
	void setBottom(int newBottomVisIdx);
	void setTop(int newTopVisIdx);
	void scroll(int amount);

	void print(char const* name); // TEMP

	std::string searchPrefix;
	Uint32 searchTime;
		
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
