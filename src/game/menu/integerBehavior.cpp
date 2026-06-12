#include "integerBehavior.hpp"

#include "../common.hpp"
#include "../gfx.hpp"
#include "../inputState.hpp"
#include "../mixer/player.hpp"
#include "../text.hpp"
#include "menu.hpp"
#include "menuItem.hpp"

#include <algorithm>
#include <cmath>

bool IntegerBehavior::OnLeftRight(Menu& menu, MenuItem& item, int dir) {
  if ((gfx.menu_cycles % scroll_interval) != 0) {
    return true;
  }

  int new_v = v;
  if ((dir < 0 && new_v > min) || (dir > 0 && new_v < max)) {
    // Clamp so a step larger than the remaining range can't overshoot the
    // bounds (e.g. the colour picker's step of 4 against max 255).
    new_v = std::clamp(new_v + dir * step, min, max);
  }

  if (new_v != v) {
    v = new_v;
    OnUpdate(menu, item);
  }

  return true;
}

static int FilterDigits(int k) { return std::isdigit(k) ? k : 0; }

int IntegerBehavior::OnEnter(Menu& menu, MenuItem& item) {
  g_sound_player->Play(common.sound_hook[SoundMenuSelect]);

  if (!allow_entry) {
    return -1;  // Not allowed
  }

  int x = 0;
  int y = 0;
  if (menu.ItemPosition(item, x, y)) {
    x += menu.value_offset_x;
    // Entry happens in displayed units; the stored value scales back up.
    int const kMinVal = min / display_div;
    int const kMaxVal = max / display_div;
    int const kDigits = 1 + static_cast<int>(std::floor(std::log10(static_cast<double>(kMaxVal))));

    int* dest_ptr = &v;
    int const kDiv = display_div;
    bool const kPct = percentage;

    gfx.state_stack.Push(std::make_unique<InputStringState>(
                             ToString(v / display_div), kDigits, x + 2, y, FilterDigits, "", false,
                             [dest_ptr, kMinVal, kMaxVal, kDiv, kPct, &menu, &item](
                                 bool accepted, std::string const& result) {
                               if (accepted && !result.empty()) {
                                 // NOLINTNEXTLINE(bugprone-unchecked-string-to-number-conversion, cert-err34-c) — input has already been filtered by FilterDigits.
                                 int val = std::atoi(result.c_str());
                                 if (val < kMinVal) {
                                   val = kMinVal;
                                 } else if (val > kMaxVal) {
                                   val = kMaxVal;
                                 }
                                 *dest_ptr = val * kDiv;
                               }
                               // Update the menu item display
                               item.value = ToString(*dest_ptr / kDiv);
                               item.has_value = true;
                               if (kPct) {
                                 item.value += "%";
                               }
                             }),
                         &gfx);
  }
  return -1;
}

void IntegerBehavior::OnUpdate(Menu& /*menu*/, MenuItem& item) {
  item.value = ToString(v / display_div);
  item.has_value = true;
  if (percentage) {
    item.value += "%";
  }
}
