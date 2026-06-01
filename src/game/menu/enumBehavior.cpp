#include "enumBehavior.hpp"

#include "../common.hpp"
#include "../mixer/player.hpp"
#include "../text.hpp"
#include "menu.hpp"
#include "menuItem.hpp"

bool EnumBehavior::onLeftRight(Menu& menu, MenuItem& item, int dir) {
  if (brokenLeftRight) return false;  // Left/right doesn't work for this item
  if (dir > 0)
    g_soundPlayer->play(common.soundHook[SoundMenuMoveUp]);
  else
    g_soundPlayer->play(common.soundHook[SoundMenuMoveDown]);

  change(menu, item, dir);

  return false;
}

int EnumBehavior::onEnter(Menu& menu, MenuItem& item) {
  g_soundPlayer->play(common.soundHook[SoundMenuSelect]);

  change(menu, item, 1);
  return -1;
}

void EnumBehavior::change(Menu& menu, MenuItem& item, int dir) {
  uint32_t range = max - min + 1;
  uint32_t newV = ((v + dir + range - min) % range) + min;

  if (newV != v) {
    v = newV;
    menu.updateItems(common);
  }
}

void EnumBehavior::onUpdate(Menu& menu, MenuItem& item) {
  item.value = toString(v);
  item.hasValue = true;
}
