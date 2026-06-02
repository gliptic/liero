#include "timeBehavior.hpp"

#include "../common.hpp"
#include "../text.hpp"
#include "menu.hpp"
#include "menuItem.hpp"

void TimeBehavior::OnUpdate(Menu& menu, MenuItem& item) {
  item.value = frames ? TimeToStringFrames(v) : TimeToString(v);
  item.has_value = true;
}
