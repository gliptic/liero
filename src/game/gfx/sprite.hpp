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
  SpriteSet() : width(0), height(0), spriteSize(0), count(0) {}

  std::vector<PalIdx> data;
  int width;
  int height;
  int spriteSize;
  int count;

  PalIdx* spritePtr(int frame) {
    assert(frame >= 0 && frame < count);
    return &data[frame * spriteSize];
  }

  Sprite operator[](int frame) {
    assert(frame >= 0 && frame < count);
    Sprite s = {&data[frame * spriteSize], width, height, width};
    return s;
  }

  void allocate(int width, int height, int count);
};
