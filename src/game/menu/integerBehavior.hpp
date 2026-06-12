#pragma once

#include "itemBehavior.hpp"

struct Common;
struct Menu;

struct IntegerBehavior : ItemBehavior {
  IntegerBehavior(Common& common, int& v, int min, int max, int step = 1, bool percentage = false)
      : common(common),
        v(v),
        min(min),
        max(max),
        step(step),

        percentage(percentage) {}

  bool OnLeftRight(Menu& menu, MenuItem& item, int dir) override;
  int OnEnter(Menu& menu, MenuItem& item) override;
  void OnUpdate(Menu& menu, MenuItem& item) override;

  Common& common;
  int& v;
  int min, max, step;
  int scroll_interval{5};
  // Displayed value = v / display_div; typed input is multiplied back.
  // Lets the classic colour picker show the VGA 0..63 range while the
  // stored channel stays 0..255.
  int display_div{1};
  bool percentage;
  bool allow_entry{true};
};
