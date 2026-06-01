#pragma once

#include <string>
#include "../gfx/color.hpp"
#include "../gfx/renderer.hpp"

struct Common;

struct MenuItem {
  MenuItem(PalIdx color, PalIdx disColour, std::string string, int id = -1)
      : color(color),
        disColour(disColour),
        string(string),
        hasValue(false),
        visible(true),
        selectable(true),
        id(id) {}

  static MenuItem space() {
    MenuItem m(0, 0, "");
    m.selectable = false;
    return m;
  }

  void draw(Common& common, Renderer& renderer, int x, int y, bool selected, bool disabled,
            bool centered, int valueOffsetX);

  PalIdx color;
  PalIdx disColour;
  std::string string;

  bool hasValue;
  std::string value;

  bool visible, selectable;
  int id;
};
