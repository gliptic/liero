#include "booleanSwitchBehavior.hpp"

#include "../common.hpp"
#include "../mixer/player.hpp"
#include "menu.hpp"
#include "menuItem.hpp"

bool BooleanSwitchBehavior::onLeftRight(Menu& menu, MenuItem& item, int dir) {
  if (dir > 0)
    g_soundPlayer->play(common.soundHook[SoundMenuMoveUp]);
  else
    g_soundPlayer->play(common.soundHook[SoundMenuMoveDown]);

  set(!v);
  onUpdate(menu, item);
  return false;
}

int BooleanSwitchBehavior::onEnter(Menu& menu, MenuItem& item) {
  g_soundPlayer->play(common.soundHook[SoundMenuSelect]);
  set(!v);
  onUpdate(menu, item);
  return -1;
}

void BooleanSwitchBehavior::onUpdate(Menu& menu, MenuItem& item) {
  item.value = common.texts.onoff[v];
  item.hasValue = true;
}
