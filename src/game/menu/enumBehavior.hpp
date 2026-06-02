#pragma once

#include "itemBehavior.hpp"

#include <cstdint>

struct Common;
struct Menu;

struct EnumBehavior : ItemBehavior {
  EnumBehavior(Common& common, uint32_t& v, uint32_t min, uint32_t max,
               bool broken_left_right = false)
      : common(common), v(v), min(min), max(max), broken_left_right(broken_left_right) {}

  bool OnLeftRight(Menu& menu, MenuItem& item, int dir);
  int OnEnter(Menu& menu, MenuItem& item);
  void OnUpdate(Menu& menu, MenuItem& item);

  void Change(Menu& menu, MenuItem& item, int dir);

  Common& common;
  uint32_t& v;
  uint32_t min, max;
  bool broken_left_right;
};
