#pragma once

#include <cstdint>

struct Material {
  enum {
    kDirt = 1 << 0,
    kDirt2 = 1 << 1,
    kRock = 1 << 2,
    kBackground = 1 << 3,
    kSeeShadow = 1 << 4,
    kWormM = 1 << 5
  };

  bool Dirt() const { return (flags & kDirt) != 0; }
  bool Dirt2() const { return (flags & kDirt2) != 0; }
  bool Rock() const { return (flags & kRock) != 0; }
  bool Background() const { return (flags & kBackground) != 0; }
  bool SeeShadow() const { return (flags & kSeeShadow) != 0; }

  // Constructed
  bool DirtRock() const { return (flags & (kDirt | kDirt2 | kRock)) != 0; }
  bool AnyDirt() const { return (flags & (kDirt | kDirt2)) != 0; }
  bool DirtBack() const { return (flags & (kDirt | kDirt2 | kBackground)) != 0; }
  bool Worm() const { return (flags & kWormM) != 0; }

  uint8_t flags;
};
