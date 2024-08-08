#include "menu.hpp"

#include "../gfx.hpp"
#include "../sfx.hpp"
#include "../reader.hpp"
#include "../text.hpp"
#include <cmath>
#include <cctype>
#include <algorithm>

#include "../common.hpp"

void Menu::onKeys(SDL_Keysym* begin, SDL_Keysym* end, bool contains)
{
	for (; begin != end; ++begin)
	{
		bool isTab = begin->scancode == SDL_SCANCODE_TAB;
		if ((begin->sym >= 32 && begin->sym <= 127) // x >= SDLK_SPACE && x <= SDLK_DELETE
		  || isTab)
		{
			Uint32 time = SDL_GetTicks();
			if (!isTab && time - searchTime > 1500)
				searchPrefix.clear();

			while (true)
			{
				bool wasEmpty = searchPrefix.empty();
				auto newPrefix = searchPrefix;

				if (!isTab)
					newPrefix += char(begin->sym);
				searchTime = time;

				bool found = false;

				int skip = isTab ? 1 : 0;

				for (std::size_t offs = skip; offs < items.size(); ++offs)
				{
					int i = (selection_ + offs) % (int)items.size();
					auto const& menuString = items[i].string;

					if (items[i].visible
					 && menuString.size() >= newPrefix.size())
					{
						bool result;


						if (contains)
						{
							result = std::search(menuString.begin(), menuString.end(), newPrefix.begin(), newPrefix.end(), [](char a, char b) {
								return std::toupper((unsigned char)a) == std::toupper((unsigned char)b);
							}) != menuString.end();
						}
						else
						{
							result = std::equal(newPrefix.begin(), newPrefix.end(), menuString.begin(), [](char a, char b) {
								return std::toupper((unsigned char)a) == std::toupper((unsigned char)b);
							});
						}

						if (result)
						{
							found = true;
							moveTo(i);
							break;
						}
					}
				}

				if (found)
				{
					searchPrefix = newPrefix;
					break;
				}

				searchPrefix.clear();
				if (wasEmpty)
					break;
			}
		}
	}
}

void Menu::draw(Common& common, Renderer& renderer, bool disabled, int x, bool showDisabledSelection)
{
	int itemsLeft = height;
	int curY = y;

	if (x < 0)
		x = this->x;

	for(int c = itemFromVisibleIndex(topItem); itemsLeft > 0 && c < (int)items.size(); ++c)
	{
		MenuItem& item = items[c];
		if(!item.visible)
			continue;

		--itemsLeft;

		bool selected = (c == selection_) && (!disabled || showDisabledSelection);

		item.draw(common, renderer, x, curY, selected, disabled, centered, valueOffsetX);
		drawItemOverlay(common, item, x, curY, selected, disabled);

		curY += itemHeight;
	}

	if(visibleItemCount > height)
	{
		int menuHeight = height * itemHeight + 1;

		common.font.drawChar(renderer.bmp, 22, x - 6, y + 2, 0);
		common.font.drawChar(renderer.bmp, 22, x - 7, y + 1, 50);
		common.font.drawChar(renderer.bmp, 23, x - 6, y + menuHeight - 7, 0);
		common.font.drawChar(renderer.bmp, 23, x - 7, y + menuHeight - 8, 50);

		int scrollBarHeight = menuHeight - 17;

		int scrollTabHeight = int(height*scrollBarHeight / visibleItemCount);
		scrollTabHeight = std::min(scrollTabHeight, scrollBarHeight);
		scrollTabHeight = std::max(scrollTabHeight, 0);
		int scrollTabY = y + int(topItem * scrollBarHeight / visibleItemCount);

		fillRect(renderer.bmp, x - 7, scrollTabY + 9, 7, scrollTabHeight, 0);
		fillRect(renderer.bmp, x - 8, scrollTabY + 8, 7, scrollTabHeight, 7);
	}
}

void Menu::moveToId(int id)
{
	moveTo(indexFromId(id));
}

void Menu::moveTo(int newSelection)
{
	newSelection = std::max(newSelection, 0);
	newSelection = std::min(newSelection, (int)items.size()-1);
	selection_ = firstVisibleFrom(newSelection);
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

bool Menu::itemPosition(MenuItem& item, int& x, int& y)
{
	int index = int(&item - &items[0]);
	if(!isInView(index))
		return false;

	int visIdx = visibleItemIndex(index);
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
		if(items[i].visible && items[i].selectable)
		{
			return (int)i;
		}
	}

	return (int)items.size();
}

int Menu::lastVisibleFrom(int item)
{
	for(std::size_t i = item; i-- > 0;)
	{
		if(items[i].visible && items[i].selectable)
		{
			return (int)i + 1;
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
	return (int)items.size();
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

void Menu::setVisibility(int id, bool state)
{
	int item = indexFromId(id);
	if (item < 0)
	{
		sassert(false);
		return;
	}

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
			if(items[i].visible && items[i].selectable)
			{
				moveTo(i);
				return;
			}
		}

		for(int i = (int)items.size() - 1; i > selection_; --i)
		{
			if(items[i].visible && items[i].selectable)
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
			if(items[i].visible && items[i].selectable)
			{
				moveTo(i);
				return;
			}
		}

		for(int i = 0; i < selection_; ++i)
		{
			if(items[i].visible && items[i].selectable)
			{
				moveTo(i);
				return;
			}
		}
	}
}

int Menu::addItem(MenuItem item)
{
	int idx = (int)items.size();
	items.push_back(item);
	if(item.visible)
		++visibleItemCount;
	return idx;
}

void Menu::clear()
{
	items.clear();
	visibleItemCount = 0;
	setTop(0);
}

int Menu::addItem(MenuItem item, int pos)
{
	int idx = (int)items.size();
	items.insert(items.begin() + pos, item);
	if(item.visible)
		++visibleItemCount;
	return idx;
}


