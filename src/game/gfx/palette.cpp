#include "palette.hpp"

#include <algorithm>

#include "../gfx.hpp"
#include "../reader.hpp"
#include "../settings.hpp"
#include "io/stream.hpp"

void Palette::Activate(Color real_pal[256]) {
  for (int i = 0; i < 256; ++i) {
    real_pal[i].r = entries[i].r << 2;
    real_pal[i].g = entries[i].g << 2;
    real_pal[i].b = entries[i].b << 2;
  }
}

static int FadeValue(int v, int amount) {
  assert(v < 64);
  v = (v * amount) >> 5;
  v = std::max(v, 0);
  return v;
}

static int LightUpValue(int v, int amount) {
  v = (v * (32 - amount) + amount * 63) >> 5;
  v = std::min(v, 63);
  return v;
}

void Palette::Fade(int amount) {
  if (amount >= 32) return;

  for (auto& entrie : entries) {
    entrie.r = FadeValue(entrie.r, amount);
    entrie.g = FadeValue(entrie.g, amount);
    entrie.b = FadeValue(entrie.b, amount);
  }
}

void Palette::LightUp(int amount) {
  for (auto& entrie : entries) {
    entrie.r = LightUpValue(entrie.r, amount);
    entrie.g = LightUpValue(entrie.g, amount);
    entrie.b = LightUpValue(entrie.b, amount);
  }
}

void Palette::RotateFrom(Palette& source, int from, int to, unsigned dist) {
  int const kCount = (to - from + 1);
  dist %= kCount;

  for (int i = 0; i < kCount; ++i) {
    entries[from + i] = source.entries[from + ((i + kCount - dist) % kCount)];
  }
}

void Palette::Clear() { std::memset(entries, 0, sizeof(entries)); }

void Palette::Read(io::Reader& r) {
  for (auto& entrie : entries) {
    uint8_t rgb[3];
    r.Get(rgb, 3);

    entrie.r = rgb[0] & 63;
    entrie.g = rgb[1] & 63;
    entrie.b = rgb[2] & 63;
  }
}

int const Palette::kWormColourIndexes[2] = {0x58, 0x78};  // TODO: Read from EXE?

// Sprite palette bases per worm index (hardcoded in worm sprite precomputation)
int const Palette::kWormSpriteColorBase[2] = {32, 41};

void Palette::SetWormColour(int i, WormSettings const& settings) {
  // Always write to the sprite-referenced palette positions for this worm index.
  // Worm sprites have hardcoded pixel values: 30-34 for worm 0, 39-43 for worm 1.
  int const kIdx = kWormSpriteColorBase[i];

  SetWormColoursSpan(kIdx, settings.rgb);

  for (int j = 0; j < 6; ++j) {
    entries[kWormColourIndexes[i] + j] = entries[kIdx + (j % 3) - 1];
  }

  for (int j = 0; j < 3; ++j) {
    entries[129 + i * 4 + j] = entries[kIdx + j];
  }
}

void Palette::SetWormColours(Settings const& settings) {
  for (int i = 0; i < 2; ++i) {
    SetWormColour(i, *settings.worm_settings[i]);
  }
}
