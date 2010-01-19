#include "menu.hpp"

#include "../gfx.hpp"
#include "../sfx.hpp"
#include "../reader.hpp"
#include "../text.hpp"
#include <cmath>

#include "../common.hpp"

void Menu::draw(Common& common/*, int x, int y*/, bool disabled)
{
	int itemsLeft = height;
	int curY = y;
	
	for(int c = itemFromVisibleIndex(topItem); itemsLeft > 0 && c < (int)items.size(); ++c)
	{
		MenuItem& item = items[c];
		if(!item.visible)
			continue;
			
		--itemsLeft;
			
		bool selected = (c == selection_) && !disabled;
			
		item.draw(common, x, curY, selected, disabled, centered, valueOffsetX);
		drawItemOverlay(common, c, x, curY, selected, disabled);
		
		curY += itemHeight;
	}
	
	if(visibleItemCount > height)
	{
		int menuHeight = height * itemHeight + 1;
		
		common.font.drawChar(22, x - 6, y + 2, 0);
		common.font.drawChar(22, x - 7, y + 1, 50);
		common.font.drawChar(23, x - 6, y + menuHeight - 7, 0);
		common.font.drawChar(23, x - 7, y + menuHeight - 8, 50);
		
		int scrollBarHeight = menuHeight - 17;
		
		int scrollTabHeight = int(height*scrollBarHeight / visibleItemCount);
		scrollTabHeight = std::min(scrollTabHeight, scrollBarHeight);
		scrollTabHeight = std::max(scrollTabHeight, 0);
		int scrollTabY = y + int(topItem * scrollBarHeight / visibleItemCount);
		
		fillRect(x - 7, scrollTabY + 9, 7, scrollTabHeight, 0);
		fillRect(x - 8, scrollTabY + 8, 7, scrollTabHeight, 7);
	}
}

void Menu::moveTo(int newSelection)
{
	newSelection = std::max(newSelection, 0);
	newSelection = std::min(newSelection, (int)items.size()-1);
	selection_ = newSelection;
	ensureInView(selection_);
}

void Menu::moveToFirstVisible()
{
	moveTo(firstVisibleFrom(0));
}

bool Menu::isInView(int item)
{
	int visibleIndex = visibleItemIndex(item);
	return visibleIndex >= topItem && visibleIndex < bottomItem;
}

bool Menu::itemPosition(int item, int& x, int& y)
{
	if(!isInView(item))
		return false;
		
	int visIdx = visibleItemIndex(item);
	x = this->x;
	y = this->y + (visIdx - topItem) * itemHeight;
	return true;
}

void Menu::ensureInView(int item)
{
	if(item < 0 || item >= (int)items.size()
	|| !items[item].visible)
		return; // Can't show items outside the menu or invisible items
		
	int visibleIndex = visibleItemIndex(item);
	
	if(visibleIndex < topItem)
		setTop(visibleIndex);
	else if(visibleIndex >= bottomItem)
		setBottom(visibleIndex + 1);
}

int Menu::firstVisibleFrom(int item)
{
	for(std::size_t i = item; i < items.size(); ++i)
	{
		if(items[i].visible)
		{
			return i;
		}
	}
	
	return items.size();
}

int Menu::lastVisibleFrom(int item)
{
	for(std::size_t i = item; i-- > 0;)
	{
		if(items[i].visible)
		{
			return i + 1;
		}
	}
	
	return 0;
}

int Menu::visibleItemIndex(int item)
{
	int idx = 0;
	for(int i = 0; i < (int)items.size(); ++i)
	{
		if(!items[i].visible)
			continue;
		
		if(i >= item)
			break;
		++idx;
	}
	return idx;
}

int Menu::itemFromVisibleIndex(int idx)
{
	for(int i = 0; i < (int)items.size(); ++i)
	{
		if(!items[i].visible)
			continue;
		
		if(idx == 0)
			return i;
		--idx;
	}
	return items.size();
}

void Menu::setBottom(int newBottomVisIdx)
{
	setTop(newBottomVisIdx - height);
	
}

void Menu::setTop(int newTopVisIdx)
{
	newTopVisIdx = std::min(newTopVisIdx, visibleItemCount - height);
	newTopVisIdx = std::max(newTopVisIdx, 0);
	topItem = newTopVisIdx;
	bottomItem = std::min(topItem + height, visibleItemCount);
}

void Menu::setVisibility(int item, bool state)
{
	if(items[item].visible && !state)
		--visibleItemCount;
	else if(!items[item].visible && state)
		++visibleItemCount;
		
	int realTopItem = itemFromVisibleIndex(topItem);

	items[item].visible = state;
#if 0 // We can't do this at every change, because it can unselect items that are hidden and then shown again
	if(!items[selection()].visible)
		movement(1);
#endif
	
	setTop(visibleItemIndex(realTopItem));
	ensureInView(selection());
}

void Menu::scroll(int dir)
{
	setTop(topItem + dir);
}

void Menu::movementPage(int direction)
{
	int sel = visibleItemIndex(selection_);
	
	int offset = direction * (height/2);
	sel += offset;
	setTop(topItem + offset);
	
	sel = std::max(sel, 0);
	sel = std::min(sel, visibleItemCount-1);
	
	moveTo(itemFromVisibleIndex(sel));
}

void Menu::movement(int direction)
{
	if(direction < 0)
	{
		for(int i = selection_ - 1; i >= 0; --i)
		{
			if(items[i].visible)
			{
				moveTo(i);
				return;
			}
		}
		
		for(int i = (int)items.size() - 1; i > selection_; --i)
		{
			if(items[i].visible)
			{
				moveTo(i);
				return;
			}
		}
	}
	else if(direction > 0)
	{
		for(int i = selection_ + 1; i < (int)items.size(); ++i)
		{
			if(items[i].visible)
			{
				moveTo(i);
				return;
			}
		}
		
		for(int i = 0; i < selection_; ++i)
		{
			if(items[i].visible)
			{
				moveTo(i);
				return;
			}
		}
	}
}

void Menu::readItems(FILE* f, int length, int count, bool colorPrefix, PalIdx color, PalIdx disColour)
{
	char temp[256];
	for(int i = 0; i < count; ++i)
	{
		checkedFread(&temp[0], 1, length, f);
		int offset = 1;
		int length = static_cast<unsigned char>(temp[0]);
		if(colorPrefix)
		{
			color = disColour = temp[2];
			length -= 2;
			offset += 2;
		}
		addItem(MenuItem(color, disColour, std::string(&temp[offset], length)));
	}
	
	setTop(0);
}

void Menu::readItem(FILE* f, int offset, PalIdx color, PalIdx disColour)
{
	addItem(MenuItem(color, disColour, readPascalStringAt(f, offset)));
}

int Menu::addItem(MenuItem item)
{
	int idx = (int)items.size();
	items.push_back(item);
	if(item.visible)
		++visibleItemCount;
	return idx;
}

int Menu::addItem(MenuItem item, int pos)
{
	int idx = (int)items.size();
	items.insert(items.begin() + pos, item);
	if(item.visible)
		++visibleItemCount;
	return idx;
}


