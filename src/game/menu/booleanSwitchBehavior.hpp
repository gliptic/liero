#pragma once

#include <functional>
#include "itemBehavior.hpp"

struct Common;
struct Menu;

struct BooleanSwitchBehavior : ItemBehavior {
  BooleanSwitchBehavior(Common& common, bool& v)
      : set([&](bool newV) { v = newV; }), common(common), v(v) {}

  BooleanSwitchBehavior(Common& common, bool& v, std::function<void(bool)> set)
      : set(set), common(common), v(v) {}

  std::function<void(bool)> set;

  bool onLeftRight(Menu& menu, MenuItem& item, int dir);
  int onEnter(Menu& menu, MenuItem& item);
  void onUpdate(Menu& menu, MenuItem& item);

  Common& common;
  bool& v;
};
