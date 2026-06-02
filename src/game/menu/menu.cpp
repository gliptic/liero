#include "menu.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include "../gfx.hpp"
#include "../mixer/player.hpp"
#include "../reader.hpp"
#include "../text.hpp"

#include "../common.hpp"

void Menu::OnKeys(SDL_Scancode* begin, SDL_Scancode* end, bool contains) {
  for (; begin != end; ++begin) {
    SDL_Scancode scancode = *begin;
    SDL_Keycode sym = SDL_GetKeyFromScancode(scancode, SDL_KMOD_NONE, false);
    bool is_tab = scancode == SDL_SCANCODE_TAB;
    if ((sym >= 32 && sym <= 127)  // x >= SDLK_SPACE && x <= SDLK_DELETE
        || is_tab) {
      auto time = SDL_GetTicks();
      if (!is_tab && time - search_time > 1500) search_prefix.clear();

      while (true) {
        bool was_empty = search_prefix.empty();
        auto new_prefix = search_prefix;

        if (!is_tab) new_prefix += char(sym);
        search_time = time;

        bool found = false;

        int skip = is_tab ? 1 : 0;

        for (std::size_t offs = skip; offs < items.size(); ++offs) {
          int i = (selection_ + offs) % (int)items.size();
          auto const& menu_string = items[i].string;

          if (items[i].visible && menu_string.size() >= new_prefix.size()) {
            bool result;

            if (contains) {
              result = std::search(menu_string.begin(), menu_string.end(), new_prefix.begin(),
                                   new_prefix.end(), [](char a, char b) {
                                     return std::toupper((unsigned char)a) ==
                                            std::toupper((unsigned char)b);
                                   }) != menu_string.end();
            } else {
              result = std::equal(
                  new_prefix.begin(), new_prefix.end(), menu_string.begin(), [](char a, char b) {
                    return std::toupper((unsigned char)a) == std::toupper((unsigned char)b);
                  });
            }

            if (result) {
              found = true;
              MoveTo(i);
              break;
            }
          }
        }

        if (found) {
          search_prefix = new_prefix;
          break;
        }

        search_prefix.clear();
        if (was_empty) break;
      }
    }
  }
}

void Menu::Draw(Common& common, Renderer& renderer, bool disabled, int x,
                bool show_disabled_selection) {
  int items_left = height;
  int cur_y = y;

  if (x < 0) x = this->x;

  for (int c = ItemFromVisibleIndex(top_item); items_left > 0 && c < (int)items.size(); ++c) {
    MenuItem& item = items[c];
    if (!item.visible) continue;

    --items_left;

    bool selected = (c == selection_) && (!disabled || show_disabled_selection);

    item.Draw(common, renderer, x, cur_y, selected, disabled, centered, value_offset_x);
    DrawItemOverlay(common, item, x, cur_y, selected, disabled);

    cur_y += item_height;
  }

  if (visible_item_count > height) {
    int menu_height = height * item_height + 1;

    common.font.DrawChar(renderer.bmp, 22, x - 6, y + 2, 0);
    common.font.DrawChar(renderer.bmp, 22, x - 7, y + 1, 50);
    common.font.DrawChar(renderer.bmp, 23, x - 6, y + menu_height - 7, 0);
    common.font.DrawChar(renderer.bmp, 23, x - 7, y + menu_height - 8, 50);

    int scroll_bar_height = menu_height - 17;

    int scroll_tab_height = int(height * scroll_bar_height / visible_item_count);
    scroll_tab_height = std::min(scroll_tab_height, scroll_bar_height);
    scroll_tab_height = std::max(scroll_tab_height, 0);
    int scroll_tab_y = y + int(top_item * scroll_bar_height / visible_item_count);

    FillRect(renderer.bmp, x - 7, scroll_tab_y + 9, 7, scroll_tab_height, 0);
    FillRect(renderer.bmp, x - 8, scroll_tab_y + 8, 7, scroll_tab_height, 7);
  }
}

void Menu::MoveToId(int id) { MoveTo(IndexFromId(id)); }

void Menu::MoveTo(int new_selection) {
  new_selection = std::max(new_selection, 0);
  new_selection = std::min(new_selection, (int)items.size() - 1);
  selection_ = FirstVisibleFrom(new_selection);
  EnsureInView(selection_);
}

void Menu::MoveToFirstVisible() { MoveTo(FirstVisibleFrom(0)); }

bool Menu::IsInView(int item) {
  int visible_index = VisibleItemIndex(item);
  return visible_index >= top_item && visible_index < bottom_item;
}

bool Menu::ItemPosition(MenuItem& item, int& x, int& y) {
  int index = int(&item - &items[0]);
  if (!IsInView(index)) return false;

  int vis_idx = VisibleItemIndex(index);
  x = this->x;
  y = this->y + (vis_idx - top_item) * item_height;
  return true;
}

void Menu::EnsureInView(int item) {
  if (item < 0 || item >= (int)items.size() || !items[item].visible)
    return;  // Can't show items outside the menu or invisible items

  int visible_index = VisibleItemIndex(item);

  if (visible_index < top_item)
    SetTop(visible_index);
  else if (visible_index >= bottom_item)
    SetBottom(visible_index + 1);
}

int Menu::FirstVisibleFrom(int item) {
  for (std::size_t i = item; i < items.size(); ++i) {
    if (items[i].visible && items[i].selectable) {
      return (int)i;
    }
  }

  return (int)items.size();
}

int Menu::LastVisibleFrom(int item) {
  for (std::size_t i = item; i-- > 0;) {
    if (items[i].visible && items[i].selectable) {
      return (int)i + 1;
    }
  }

  return 0;
}

int Menu::VisibleItemIndex(int item) {
  int idx = 0;
  for (int i = 0; i < (int)items.size(); ++i) {
    if (!items[i].visible) continue;

    if (i >= item) break;
    ++idx;
  }
  return idx;
}

int Menu::ItemFromVisibleIndex(int idx) {
  for (int i = 0; i < (int)items.size(); ++i) {
    if (!items[i].visible) continue;

    if (idx == 0) return i;
    --idx;
  }
  return (int)items.size();
}

void Menu::SetBottom(int new_bottom_vis_idx) { SetTop(new_bottom_vis_idx - height); }

void Menu::SetTop(int new_top_vis_idx) {
  new_top_vis_idx = std::min(new_top_vis_idx, visible_item_count - height);
  new_top_vis_idx = std::max(new_top_vis_idx, 0);
  top_item = new_top_vis_idx;
  bottom_item = std::min(top_item + height, visible_item_count);
}

void Menu::SetVisibility(int id, bool state) {
  int item = IndexFromId(id);
  if (item < 0) {
    assert(false);
    return;
  }

  if (items[item].visible && !state)
    --visible_item_count;
  else if (!items[item].visible && state)
    ++visible_item_count;

  int real_top_item = ItemFromVisibleIndex(top_item);

  items[item].visible = state;

  SetTop(VisibleItemIndex(real_top_item));
  EnsureInView(Selection());
}

void Menu::Scroll(int dir) { SetTop(top_item + dir); }

void Menu::MovementPage(int direction) {
  int sel = VisibleItemIndex(selection_);

  int offset = direction * (height / 2);
  sel += offset;
  SetTop(top_item + offset);

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

    for (int i = (int)items.size() - 1; i > selection_; --i) {
      if (items[i].visible && items[i].selectable) {
        MoveTo(i);
        return;
      }
    }
  } else if (direction > 0) {
    for (int i = selection_ + 1; i < (int)items.size(); ++i) {
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

int Menu::AddItem(MenuItem item) {
  int idx = (int)items.size();
  items.push_back(item);
  if (item.visible) ++visible_item_count;
  return idx;
}

void Menu::Clear() {
  items.clear();
  visible_item_count = 0;
  SetTop(0);
}

int Menu::AddItem(MenuItem item, int pos) {
  int idx = (int)items.size();
  items.insert(items.begin() + pos, item);
  if (item.visible) ++visible_item_count;
  return idx;
}
