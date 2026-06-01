#pragma once

#include <vector>

struct Menu;
struct MenuItem;

struct ItemBehavior {
  ItemBehavior() {}

  virtual ~ItemBehavior() {}

  virtual bool onLeftRight(Menu& menu, MenuItem& item, int dir) { return true; }

  virtual int onEnter(Menu& menu, MenuItem& item) { return -1; }

  virtual void onUpdate(Menu& menu, MenuItem& item) {}
};
