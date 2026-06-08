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
  bool percentage;
  bool allow_entry{true};
};
