#pragma once

#include <cassert>
#include <cstdio>
#include <vector>
#include "color.hpp"

struct Sprite {
  PalIdx* mem;
  int width, height, pitch;
};

struct SpriteSet {
  SpriteSet() = default;

  std::vector<PalIdx> data;
  int width{0};
  int height{0};
  int sprite_size{0};
  int count{0};

  PalIdx* SpritePtr(int frame) {
    assert(frame >= 0 && frame < count);
    return &data[frame * sprite_size];
  }

  Sprite operator[](int frame) {
    assert(frame >= 0 && frame < count);
    Sprite s = {
        .mem = &data[frame * sprite_size], .width = width, .height = height, .pitch = width};
    return s;
  }

  void Allocate(int width, int height, int count);
};
