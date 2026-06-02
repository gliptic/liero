#pragma once

#include <vector>

struct Menu;
struct MenuItem;

struct ItemBehavior {
  ItemBehavior() {}

  virtual ~ItemBehavior() {}

  virtual bool OnLeftRight(Menu& menu, MenuItem& item, int dir) { return true; }

  virtual int OnEnter(Menu& menu, MenuItem& item) { return -1; }

  virtual void OnUpdate(Menu& menu, MenuItem& item) {}
};
