#include "palette.hpp"

#include <algorithm>

#include "../gfx.hpp"
#include "../reader.hpp"
#include "../settings.hpp"
#include "io/stream.hpp"

void Palette::Activate(Color real_pal[256]) {
  for (int i = 0; i < 256; ++i) {
    real_pal[i].r = entries[i].r;
    real_pal[i].g = entries[i].g;
    real_pal[i].b = entries[i].b;
  }
}

static int FadeValue(int v, int amount) {
  v = (v * amount) >> 5;
  v = std::max(v, 0);
  return v;
}

static int LightUpValue(int v, int amount) {
  v = (v * (32 - amount) + amount * 255) >> 5;
  v = std::min(v, 255);
  return v;
}

void Palette::Fade(int amount) {
  if (amount >= 32) {
    return;
  }

  for (auto& entry : entries) {
    entry.r = FadeValue(entry.r, amount);
    entry.g = FadeValue(entry.g, amount);
    entry.b = FadeValue(entry.b, amount);
  }
}

void Palette::LightUp(int amount) {
  for (auto& entry : entries) {
    entry.r = LightUpValue(entry.r, amount);
    entry.g = LightUpValue(entry.g, amount);
    entry.b = LightUpValue(entry.b, amount);
  }
}

void Palette::RotateFrom(Palette const& source, int from, int to, unsigned dist) {
  int const kCount = (to - from + 1);
  dist %= kCount;

  for (int i = 0; i < kCount; ++i) {
    entries[from + i] = source.entries[from + ((i + kCount - dist) % kCount)];
  }
}

void Palette::Clear() { std::memset(entries, 0, sizeof(entries)); }

void Palette::Read(io::Reader& r) {
  // Classic palette files carry 6-bit VGA channels; expand to the 8-bit
  // range entries hold, preserving the legacy (v & 63) << 2 screen values.
  for (auto& entry : entries) {
    uint8_t rgb[3];
    r.Get(rgb, 3);

    entry.r = (rgb[0] & 63) << 2;
    entry.g = (rgb[1] & 63) << 2;
    entry.b = (rgb[2] & 63) << 2;
  }
}

// Worm sprites have hardcoded pixel values: 30-34 for worm 0, 39-43 for
// worm 1, with secondary copies at 0x58 / 0x78 and minimap / status colours
// at 129 / 133. TODO: Read from EXE?
ColorBlock const Palette::kWormColorBlocks[2] = {
    {.base = 32, .colour_index = 0x58, .status_index = 129},
    {.base = 41, .colour_index = 0x78, .status_index = 133}};

void Palette::ReadFull(io::Reader& r) {
  for (auto& entry : entries) {
    uint8_t rgb[3];
    r.Get(rgb, 3);

    entry.r = rgb[0];
    entry.g = rgb[1];
    entry.b = rgb[2];
  }
}

void Palette::SetWormColour(int i, WormSettings const& settings, ColorMode mode) {
  ColorBlock const& block = kWormColorBlocks[i];

  SetWormColoursSpan(block.base, settings.rgb, mode);

  for (int j = 0; j < 6; ++j) {
    entries[block.colour_index + j] = entries[block.base + (j % 3) - 1];
  }

  for (int j = 0; j < 3; ++j) {
    entries[block.status_index + j] = entries[block.base + j];
  }
}

void Palette::ExpandToFullRange() {
  for (auto& e : entries) {
    e.r |= e.r >> 6;
    e.g |= e.g >> 6;
    e.b |= e.b >> 6;
  }
}

void Palette::SetWormColours(Settings const& settings, ColorMode mode) {
  for (int i = 0; i < 2; ++i) {
    SetWormColour(i, *settings.worm_settings[i], mode);
  }
}
