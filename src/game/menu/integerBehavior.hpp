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
        scroll_interval(5),
        percentage(percentage),
        allow_entry(true) {}

  bool OnLeftRight(Menu& menu, MenuItem& item, int dir);
  int OnEnter(Menu& menu, MenuItem& item);
  void OnUpdate(Menu& menu, MenuItem& item);

  Common& common;
  int& v;
  int min, max, step;
  int scroll_interval;
  bool percentage;
  bool allow_entry;
};
