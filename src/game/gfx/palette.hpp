#pragma once

#include <cassert>
#include <cstdint>
#include "color.hpp"

struct Settings;
struct WormSettings;

namespace io {
struct Reader;
}

struct Palette {
  static int const kWormColourIndexes[2];
  static int const kWormSpriteColorBase[2];

  Color entries[256];

  void Activate(Color real_pal[256]);
  void Fade(int amount);
  void LightUp(int amount);
  void RotateFrom(Palette& source, int from, int to, unsigned dist);
  void Read(io::Reader& r);

  void ScaleAdd(int dest, int const (&c)[3], int scale, int add) {
    entries[dest].r = (add + c[0] * scale) / 64;
    entries[dest].g = (add + c[1] * scale) / 64;
    entries[dest].b = (add + c[2] * scale) / 64;

    assert(entries[dest].r < 64);
    assert(entries[dest].g < 64);
    assert(entries[dest].b < 64);
  }

  void SetWormColoursSpan(int base, int const (&c)[3]) {
    ScaleAdd(base - 2, c, 38, 0);
    ScaleAdd(base - 1, c, 50, 0);
    ScaleAdd(base, c, 64, 0);
    ScaleAdd(base + 1, c, 47, 1008);
    ScaleAdd(base + 2, c, 28, 2205);
  }

  void ResetPalette(Palette const& new_pal, Settings const& settings) {
    *this = new_pal;
    // setWormColours(settings);
  }

  void SetWormColour(int i, WormSettings const& settings);
  void SetWormColours(Settings const& settings);

  void Clear();
};
