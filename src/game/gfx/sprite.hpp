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
  SpriteSet() : width(0), height(0), sprite_size(0), count(0) {}

  std::vector<PalIdx> data;
  int width;
  int height;
  int sprite_size;
  int count;

  PalIdx* SpritePtr(int frame) {
    assert(frame >= 0 && frame < count);
    return &data[frame * sprite_size];
  }

  Sprite operator[](int frame) {
    assert(frame >= 0 && frame < count);
    Sprite s = {&data[frame * sprite_size], width, height, width};
    return s;
  }

  void Allocate(int width, int height, int count);
};
