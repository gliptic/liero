#include "menu.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <utility>
#include "../gfx.hpp"
#include "../mixer/player.hpp"
#include "../reader.hpp"
#include "../text.hpp"

#include "../common.hpp"

void Menu::OnKeys(SDL_Scancode* begin, const SDL_Scancode* end, bool contains) {
  for (; begin != end; ++begin) {
    SDL_Scancode const kScancode = *begin;
    SDL_Keycode const kSym = SDL_GetKeyFromScancode(kScancode, SDL_KMOD_NONE, false);
    bool const kIsTab = kScancode == SDL_SCANCODE_TAB;
    if ((kSym >= 32 && kSym <= 127)  // x >= SDLK_SPACE && x <= SDLK_DELETE
        || kIsTab) {
      auto time = SDL_GetTicks();
      if (!kIsTab && time - search_time > 1500) search_prefix.clear();

      while (true) {
        bool const kWasEmpty = search_prefix.empty();
        auto new_prefix = search_prefix;

        if (!kIsTab) new_prefix += static_cast<char>(kSym);
        search_time = time;

        bool found = false;

        int const kSkip = kIsTab ? 1 : 0;

        for (std::size_t offs = kSkip; offs < items.size(); ++offs) {
          int const kI = (selection_ + offs) % static_cast<int>(items.size());
          auto const& menu_string = items[kI].string;

          if (items[kI].visible && menu_string.size() >= new_prefix.size()) {
            bool result = false;

            if (contains) {
              result = std::ranges::search(menu_string, new_prefix, [](char a, char b) {
                         return std::toupper(static_cast<unsigned char>(a)) ==
                                std::toupper(static_cast<unsigned char>(b));
                       }).begin() != menu_string.end();
            } else {
              result = std::equal(new_prefix.begin(), new_prefix.end(), menu_string.begin(),
                                  [](char a, char b) {
                                    return std::toupper(static_cast<unsigned char>(a)) ==
                                           std::toupper(static_cast<unsigned char>(b));
                                  });
            }

            if (result) {
              found = true;
              MoveTo(kI);
              break;
            }
          }
        }

        if (found) {
          search_prefix = new_prefix;
          break;
        }

        search_prefix.clear();
        if (kWasEmpty) break;
      }
    }
  }
}

void Menu::Draw(Common& common, Renderer& renderer, bool disabled, int x,
                bool show_disabled_selection) {
  int items_left = height;
  int cur_y = y;

  if (x < 0) x = this->x;

  for (int c = ItemFromVisibleIndex(top_item); items_left > 0 && std::cmp_less(c, items.size());
       ++c) {
    MenuItem& item = items[c];
    if (!item.visible) continue;

    --items_left;

    bool const kSelected = (c == selection_) && (!disabled || show_disabled_selection);

    item.Draw(common, renderer, x, cur_y, kSelected, disabled, centered, value_offset_x);
    DrawItemOverlay(common, item, x, cur_y, kSelected, disabled);

    cur_y += item_height;
  }

  if (visible_item_count > height) {
    int const kMenuHeight = height * item_height + 1;

    common.font.DrawChar(renderer.bmp, 22, x - 6, y + 2, 0);
    common.font.DrawChar(renderer.bmp, 22, x - 7, y + 1, 50);
    common.font.DrawChar(renderer.bmp, 23, x - 6, y + kMenuHeight - 7, 0);
    common.font.DrawChar(renderer.bmp, 23, x - 7, y + kMenuHeight - 8, 50);

    int const kScrollBarHeight = kMenuHeight - 17;

    int scroll_tab_height = (height * kScrollBarHeight / visible_item_count);
    scroll_tab_height = std::min(scroll_tab_height, kScrollBarHeight);
    scroll_tab_height = std::max(scroll_tab_height, 0);
    int const kScrollTabY = y + (top_item * kScrollBarHeight / visible_item_count);

    FillRect(renderer.bmp, x - 7, kScrollTabY + 9, 7, scroll_tab_height, 0);
    FillRect(renderer.bmp, x - 8, kScrollTabY + 8, 7, scroll_tab_height, 7);
  }
}

void Menu::MoveToId(int id) { MoveTo(IndexFromId(id)); }

void Menu::MoveTo(int new_selection) {
  new_selection = std::max(new_selection, 0);
  new_selection = std::min(new_selection, static_cast<int>(items.size()) - 1);
  selection_ = FirstVisibleFrom(new_selection);
  EnsureInView(selection_);
}

void Menu::MoveToFirstVisible() { MoveTo(FirstVisibleFrom(0)); }

bool Menu::IsInView(int item) {
  int const kVisibleIndex = VisibleItemIndex(item);
  return kVisibleIndex >= top_item && kVisibleIndex < bottom_item;
}

bool Menu::ItemPosition(MenuItem& item, int& x, int& y) {
  int const kIndex = static_cast<int>(&item - items.data());
  if (!IsInView(kIndex)) return false;

  int const kVisIdx = VisibleItemIndex(kIndex);
  x = this->x;
  y = this->y + (kVisIdx - top_item) * item_height;
  return true;
}

void Menu::EnsureInView(int item) {
  if (item < 0 || std::cmp_greater_equal(item, items.size()) || !items[item].visible)
    return;  // Can't show items outside the menu or invisible items

  int const kVisibleIndex = VisibleItemIndex(item);

  if (kVisibleIndex < top_item)
    SetTop(kVisibleIndex);
  else if (kVisibleIndex >= bottom_item)
    SetBottom(kVisibleIndex + 1);
}

int Menu::FirstVisibleFrom(int item) {
  for (std::size_t i = item; i < items.size(); ++i) {
    if (items[i].visible && items[i].selectable) {
      return static_cast<int>(i);
    }
  }

  return static_cast<int>(items.size());
}

int Menu::LastVisibleFrom(int item) {
  for (std::size_t i = item; i-- > 0;) {
    if (items[i].visible && items[i].selectable) {
      return static_cast<int>(i) + 1;
    }
  }

  return 0;
}

int Menu::VisibleItemIndex(int item) {
  int idx = 0;
  for (int i = 0; std::cmp_less(i, items.size()); ++i) {
    if (!items[i].visible) continue;

    if (i >= item) break;
    ++idx;
  }
  return idx;
}

int Menu::ItemFromVisibleIndex(int idx) {
  for (int i = 0; std::cmp_less(i, items.size()); ++i) {
    if (!items[i].visible) continue;

    if (idx == 0) return i;
    --idx;
  }
  return static_cast<int>(items.size());
}

void Menu::SetBottom(int new_bottom_vis_idx) { SetTop(new_bottom_vis_idx - height); }

void Menu::SetTop(int new_top_vis_idx) {
  new_top_vis_idx = std::min(new_top_vis_idx, visible_item_count - height);
  new_top_vis_idx = std::max(new_top_vis_idx, 0);
  top_item = new_top_vis_idx;
  bottom_item = std::min(top_item + height, visible_item_count);
}

void Menu::SetVisibility(int id, bool state) {
  int const kItem = IndexFromId(id);
  if (kItem < 0) {
    assert(false);
    return;
  }

  if (items[kItem].visible && !state)
    --visible_item_count;
  else if (!items[kItem].visible && state)
    ++visible_item_count;

  int const kRealTopItem = ItemFromVisibleIndex(top_item);

  items[kItem].visible = state;

  SetTop(VisibleItemIndex(kRealTopItem));
  EnsureInView(Selection());
}

void Menu::Scroll(int dir) { SetTop(top_item + dir); }

void Menu::MovementPage(int direction) {
  int sel = VisibleItemIndex(selection_);

  int const kOffset = direction * (height / 2);
  sel += kOffset;
  SetTop(top_item + kOffset);

  sel = std::max(sel, 0);
  sel = std::min(sel, visible_item_count - 1);

  MoveTo(ItemFromVisibleIndex(sel));
}

void Menu::Movement(int direction) {
  if (direction < 0) {
    for (int i = selection_ - 1; i >= 0; --i) {
      if (items[i].visible && items[i].selectable) {
        MoveTo(i);
        return;
      }
    }

    for (int i = static_cast<int>(items.size()) - 1; i > selection_; --i) {
      if (items[i].visible && items[i].selectable) {
        MoveTo(i);
        return;
      }
    }
  } else if (direction > 0) {
    for (int i = selection_ + 1; std::cmp_less(i, items.size()); ++i) {
      if (items[i].visible && items[i].selectable) {
        MoveTo(i);
        return;
      }
    }

    for (int i = 0; i < selection_; ++i) {
      if (items[i].visible && items[i].selectable) {
        MoveTo(i);
        return;
      }
    }
  }
}

int Menu::AddItem(const MenuItem& item) {
  int const kIdx = static_cast<int>(items.size());
  items.push_back(item);
  if (item.visible) ++visible_item_count;
  return kIdx;
}

void Menu::Clear() {
  items.clear();
  visible_item_count = 0;
  SetTop(0);
}

int Menu::AddItem(const MenuItem& item, int pos) {
  int const kIdx = static_cast<int>(items.size());
  items.insert(items.begin() + pos, item);
  if (item.visible) ++visible_item_count;
  return kIdx;
}
