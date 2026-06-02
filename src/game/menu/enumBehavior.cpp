#include "enumBehavior.hpp"

#include "../common.hpp"
#include "../mixer/player.hpp"
#include "../text.hpp"
#include "menu.hpp"
#include "menuItem.hpp"

bool EnumBehavior::OnLeftRight(Menu& menu, MenuItem& item, int dir) {
  if (broken_left_right) return false;  // Left/right doesn't work for this item
  if (dir > 0)
    g_sound_player->Play(common.sound_hook[SoundMenuMoveUp]);
  else
    g_sound_player->Play(common.sound_hook[SoundMenuMoveDown]);

  Change(menu, item, dir);

  return false;
}

int EnumBehavior::OnEnter(Menu& menu, MenuItem& item) {
  g_sound_player->Play(common.sound_hook[SoundMenuSelect]);

  Change(menu, item, 1);
  return -1;
}

void EnumBehavior::Change(Menu& menu, MenuItem& item, int dir) {
  uint32_t range = max - min + 1;
  uint32_t new_v = ((v + dir + range - min) % range) + min;

  if (new_v != v) {
    v = new_v;
    menu.UpdateItems(common);
  }
}

void EnumBehavior::OnUpdate(Menu& menu, MenuItem& item) {
  item.value = ToString(v);
  item.has_value = true;
}
