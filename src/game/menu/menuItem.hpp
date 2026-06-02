#pragma once

#include <string>
#include "../gfx/color.hpp"
#include "../gfx/renderer.hpp"

struct Common;

struct MenuItem {
  MenuItem(PalIdx color, PalIdx dis_colour, std::string string, int id = -1)
      : color(color),
        dis_colour(dis_colour),
        string(string),
        has_value(false),
        visible(true),
        selectable(true),
        id(id) {}

  static MenuItem Space() {
    MenuItem m(0, 0, "");
    m.selectable = false;
    return m;
  }

  void Draw(Common& common, Renderer& renderer, int x, int y, bool selected, bool disabled,
            bool centered, int value_offset_x);

  PalIdx color;
  PalIdx dis_colour;
  std::string string;

  bool has_value;
  std::string value;

  bool visible, selectable;
  int id;
};
