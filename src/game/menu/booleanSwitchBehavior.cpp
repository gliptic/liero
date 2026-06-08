#include "booleanSwitchBehavior.hpp"

#include "../common.hpp"
#include "../mixer/player.hpp"
#include "menu.hpp"
#include "menuItem.hpp"

bool BooleanSwitchBehavior::OnLeftRight(Menu& menu, MenuItem& item, int dir) {
  if (dir > 0)
    g_sound_player->Play(common.sound_hook[SoundMenuMoveUp]);
  else
    g_sound_player->Play(common.sound_hook[SoundMenuMoveDown]);

  set(!v);
  OnUpdate(menu, item);
  return false;
}

int BooleanSwitchBehavior::OnEnter(Menu& menu, MenuItem& item) {
  g_sound_player->Play(common.sound_hook[SoundMenuSelect]);
  set(!v);
  OnUpdate(menu, item);
  return -1;
}

void BooleanSwitchBehavior::OnUpdate(Menu& /*menu*/, MenuItem& item) {
  item.value = common.texts.onoff[v];
  item.has_value = true;
}
