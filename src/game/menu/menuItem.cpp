#include "menuItem.hpp"

#include "../common.hpp"
#include "../gfx.hpp"

void MenuItem::Draw(Common& common, Renderer& renderer, int x, int y, bool selected, bool disabled,
                    bool centered, int value_offset_x) {
  int wid = common.font.GetDims(string);
  int value_wid = common.font.GetDims(value);
  if (centered) x -= (wid >> 1);

  if (selected) {
    DrawRoundedBox(renderer.bmp, x, y, 0, 7, wid);
    if (has_value)
      DrawRoundedBox(renderer.bmp, x + value_offset_x - (value_wid >> 1), y, 0, 7, value_wid);
  } else {
    common.font.DrawString(renderer.bmp, string, x + 3, y + 2, 0);
    if (has_value)
      common.font.DrawString(renderer.bmp, value, x + value_offset_x - (value_wid >> 1) + 3, y + 2,
                             0);
  }

  PalIdx c;

  if (disabled)
    c = dis_colour;
  else if (selected)
    c = disabled ? 7 : 168;
  else
    c = color;

  common.font.DrawString(renderer.bmp, string, x + 2, y + 1, c);
  if (has_value)
    common.font.DrawString(renderer.bmp, value, x + value_offset_x - (value_wid >> 1) + 2, y + 1,
                           c);
}
