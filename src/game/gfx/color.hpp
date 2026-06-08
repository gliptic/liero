#pragma once

#include <cstdint>

using PalIdx = unsigned char;

using Color = struct Color {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t unused;
};
