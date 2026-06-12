#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include "color.hpp"

struct Settings;
struct WormSettings;

namespace io {
struct Reader;
}  // namespace io

// Per-renderer colour pipeline mode. Classic reproduces the VGA-era output
// byte for byte; Modern uses the full 8-bit range (richer palette, worm
// shading computed at full precision).
enum class ColorMode : uint8_t { kClassic, kModern };

// Where a worm's colours live in the palette. Worm sprite art hardcodes
// pixel values in the ramp, so the defaults must match the shipped sprites;
// the indirection exists so future skins can claim different blocks.
struct ColorBlock {
  int base;          // centre of the shaded sprite ramp (pixels use base-2 .. base+2)
  int colour_index;  // start of the secondary 6-entry copy block
  int status_index;  // start of the 3-entry minimap / status copy
  int width;         // ramp entries centred on base
};

struct Palette {
  static ColorBlock const kWormColorBlocks[2];

  // Full 8-bit per channel. Classic sources (VGA palette files, sprite TGA
  // palettes) are expanded from 6-bit at load time.
  Color entries[256];

  void Activate(Color real_pal[256]);
  void Fade(int amount);
  void LightUp(int amount);
  void RotateFrom(Palette const& source, int from, int to, unsigned dist);
  void Read(io::Reader& r);
  // Reads a palette that already carries full 8-bit channels (no VGA clamp).
  void ReadFull(io::Reader& r);

  // `c` channels are 0..255 (clamped here — configs and net peers can carry
  // out-of-range values). Classic quantizes to the VGA grid and runs the
  // ramp math in 6-bit precision, keeping worm shading byte-identical to
  // the VGA pipeline; Modern runs it at full 8-bit precision.
  void ScaleAdd(int dest, int const (&c)[3], int scale, int add, ColorMode mode) {
    if (mode == ColorMode::kModern) {
      entries[dest].r = ScaleAddChannel8(c[0], scale, add);
      entries[dest].g = ScaleAddChannel8(c[1], scale, add);
      entries[dest].b = ScaleAddChannel8(c[2], scale, add);
      return;
    }

    int const kR = (add + (std::clamp(c[0], 0, 255) >> 2) * scale) / 64;
    int const kG = (add + (std::clamp(c[1], 0, 255) >> 2) * scale) / 64;
    int const kB = (add + (std::clamp(c[2], 0, 255) >> 2) * scale) / 64;

    assert(kR < 64);
    assert(kG < 64);
    assert(kB < 64);

    entries[dest].r = kR << 2;
    entries[dest].g = kG << 2;
    entries[dest].b = kB << 2;
  }

  void SetWormColoursSpan(int base, int const (&c)[3], ColorMode mode) {
    // Hand-tuned 64-step gradient deriving the 5-entry shaded ramp from a
    // single base colour.
    static constexpr struct {
      int scale, add;
    } kSteps[5] = {{.scale = 38, .add = 0},
                   {.scale = 50, .add = 0},
                   {.scale = 64, .add = 0},
                   {.scale = 47, .add = 1008},
                   {.scale = 28, .add = 2205}};

    int const kInput[3] = {std::clamp(c[0], 0, 255), std::clamp(c[1], 0, 255),
                           std::clamp(c[2], 0, 255)};

    for (int j = 0; j < 5; ++j) {
      ScaleAdd(base - 2 + j, kInput, kSteps[j].scale, kSteps[j].add, mode);
    }
  }

  void ResetPalette(Palette const& new_pal, Settings const& /*settings*/) {
    *this = new_pal;
    // setWormColours(settings);
  }

  void SetWormColour(int i, WormSettings const& settings, ColorMode mode);
  void SetWormColours(Settings const& settings, ColorMode mode);

  // Expands a classic palette in place from the VGA grid to the full 8-bit
  // range ((v << 2) | (v >> 4), so the brightest VGA white maps to 255).
  // Used to derive the modern palette when a TC ships no modern.pal;
  // colours stay true to the original.
  void ExpandToFullRange();

  void Clear();

 private:
  // The legacy gradient constants are tuned for 6-bit values; `add` scales
  // by 4 to land in the 8-bit range.
  static uint8_t ScaleAddChannel8(int c, int scale, int add) {
    int const kV = (4 * add + std::clamp(c, 0, 255) * scale) / 64;
    return static_cast<uint8_t>(kV < 255 ? kV : 255);
  }
};
