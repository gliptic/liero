#pragma once

#include <functional>
#include "itemBehavior.hpp"

struct Common;
struct Menu;

struct BooleanSwitchBehavior : ItemBehavior {
  BooleanSwitchBehavior(Common& common, bool& v)
      : set([&](bool new_v) { v = new_v; }), common(common), v(v) {}

  BooleanSwitchBehavior(Common& common, bool& v, std::function<void(bool)> set)
      : set(set), common(common), v(v) {}

  std::function<void(bool)> set;

  bool OnLeftRight(Menu& menu, MenuItem& item, int dir);
  int OnEnter(Menu& menu, MenuItem& item);
  void OnUpdate(Menu& menu, MenuItem& item);

  Common& common;
  bool& v;
};
