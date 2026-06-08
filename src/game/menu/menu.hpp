#pragma once

#include <SDL3/SDL.h>
#include <cstddef>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "../gfx/color.hpp"
#include "../gfx/renderer.hpp"

#include <cstdint>

#include "itemBehavior.hpp"
#include "menuItem.hpp"

#include "booleanSwitchBehavior.hpp"
#include "enumBehavior.hpp"
#include "integerBehavior.hpp"
#include "timeBehavior.hpp"

struct Common;
struct Gfx;

struct Menu {
  Menu(bool centered = false) { Init(centered); }

  Menu(int x, int y, bool centered = false) {
    Init(centered);
    Place(x, y);
  }

  virtual ~Menu() = default;

  void Init(bool centered_init = false) {
    item_height = 8;
    centered = centered_init;
    selection_ = 0;
    value_offset_x = 0;
    x = 0;
    y = 0;
    height = 15;
    top_item = 0;
    bottom_item = 0;
    // showScroll = false;
    visible_item_count = 0;
    search_time = 0;
  }

  void Draw(Common& common, Renderer& renderer, bool disabled, int x = -1,
            bool show_disabled_selection = false);
  void Process();

  virtual void DrawItemOverlay(Common& common, MenuItem& item, int x, int y, bool selected,
                               bool disabled) {
    // Nothing by default
  }

  virtual ItemBehavior* GetItemBehavior(Common& /*common*/, MenuItem& /*item*/) {
    // Dummy item behavior by default
    return new ItemBehavior;
  }

  virtual void OnUpdate() {
    // Nothing by default
  }

  bool OnLeftRight(Common& common, int dir) {
    auto* s = Selected();
    if (!s) return false;
    std::unique_ptr<ItemBehavior> b(GetItemBehavior(common, *s));
    return b->OnLeftRight(*this, *s, dir);
  }

  int OnEnter(Common& common) {
    auto* s = Selected();
    if (!s) return false;
    std::unique_ptr<ItemBehavior> b(GetItemBehavior(common, *s));
    return b->OnEnter(*this, *s);
  }

  void OnKeys(SDL_Scancode* begin, const SDL_Scancode* end, bool contains = false);

  void UpdateItems(Common& common) {
    for (auto& item : items) {
      std::unique_ptr<ItemBehavior> b(GetItemBehavior(common, item));

      b->OnUpdate(*this, item);
    }

    OnUpdate();
  }

  void Place(int new_x, int new_y) {
    x = new_x;
    y = new_y;
  }

  bool IsSelectionValid() const {
    return selection_ >= 0 && std::cmp_less(selection_, items.size());
  }

  void MoveToFirstVisible();
  void Movement(int direction);
  void MovementPage(int direction);

  int AddItem(const MenuItem& item);
  int AddItem(const MenuItem& item, int pos);
  void Clear();

  bool ItemPosition(MenuItem& item, int& x, int& y);

  int VisibleItemIndex(int item);
  int ItemFromVisibleIndex(int idx);

  void SetHeight(int new_height) {
    height = new_height;
    SetTop(top_item);
  }

  int Selection() const { return selection_; }

  // Used by the rollback weapon-select snapshot restore. Sets the
  // selection_ field directly, bypassing visibility/scroll adjustments.
  // Callers are expected to also restore topItem/bottomItem from the
  // same snapshot.
  void SetSelection(int new_selection) { selection_ = new_selection; }

  MenuItem* Selected() {
    if (!IsSelectionValid()) return nullptr;
    return &items[selection_];
  }

  int SelectedId() {
    MenuItem const* s = Selected();
    return s ? s->id : -1;
  }

  int IndexFromId(int id) {
    for (int i = 0; std::cmp_less(i, items.size()); ++i) {
      if (items[i].id == id) return i;
    }

    return -1;
  }

  MenuItem* ItemFromId(int id) {
    for (auto& item : items) {
      if (item.id == id) return &item;
    }

    return nullptr;
  }

  void SetVisibility(int id, bool state);
  int FirstVisibleFrom(int item);
  int LastVisibleFrom(int item);
  void MoveTo(int new_selection);
  void MoveToId(int id);
  bool IsInView(int item);
  void EnsureInView(int item);
  void SetBottom(int new_bottom_vis_idx);
  void SetTop(int new_top_vis_idx);
  void Scroll(int dir);

  std::string search_prefix;
  Uint64 search_time;

  std::vector<MenuItem> items;
  int item_height;
  int value_offset_x;

  int x, y;
  int height;

  int top_item;     // Visible index
  int bottom_item;  // Visible index
  // bool showScroll;

  int visible_item_count;

  bool centered;

 private:
  int selection_;  // Global index
};
