#pragma once

#include <functional>
#include <utility>
#include "itemBehavior.hpp"

struct Common;
struct Menu;

struct BooleanSwitchBehavior : ItemBehavior {
  BooleanSwitchBehavior(Common& common, bool& v)
      : set([&](bool new_v) { v = new_v; }), common(common), v(v) {}

  BooleanSwitchBehavior(Common& common, bool& v, std::function<void(bool)> set)
      : set(std::move(std::move(set))), common(common), v(v) {}

  std::function<void(bool)> set;

  bool OnLeftRight(Menu& menu, MenuItem& item, int dir) override;
  int OnEnter(Menu& menu, MenuItem& item) override;
  void OnUpdate(Menu& menu, MenuItem& item) override;

  Common& common;
  bool& v;
};
