#pragma once

#include <cstdint>

typedef unsigned char PalIdx;

typedef struct Color {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t unused;
} Color;
