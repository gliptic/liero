#pragma once

#include <cstdio>
#include <string>
#include <utility>
#include <vector>
#include "common.hpp"
#include "gfx/palette.hpp"
#include "material.hpp"
#include "math/rect.hpp"

namespace io {
struct Reader;
}

struct Game;
struct Settings;
struct Rand;
struct Common;

struct Level {
  Level(Common& common) : width(0), height(0) { zero_material = common.materials[0]; }

  bool load(Common& common, Settings const& settings, io::Reader& r);

  void GenerateDirtPattern(Common& common, Rand& rand);
  void GenerateRandom(Common& common, Settings const& settings, Rand& rand);
  void MakeShadow(Common& common);
  void GenerateFromSettings(Common& common, Settings const& settings, Rand& rand);
  bool SelectSpawn(Rand& rand, int w, int h, IVec2& selected);
  void DrawMiniature(Bitmap& dest, int map_x, int map_y, int step);

  unsigned char Pixel(int x, int y) { return data[x + y * width]; }

  unsigned char Pixel(fixedvec pos) { return data[pos.x + pos.y * width]; }

  unsigned char* Pixelp(int x, int y) { return &data[x + y * width]; }

  void SetPixel(int x, int y, PalIdx w, Common& common) {
    data[x + y * width] = w;
    materials[x + y * width] = common.materials[w];
  }

  void SetPixel(fixedvec pos, PalIdx w, Common& common) {
    data[pos.x + pos.y * width] = w;
    materials[pos.x + pos.y * width] = common.materials[w];
  }

  Material& Mat(int x, int y) { return materials[x + y * width]; }

  Material& Mat(fixedvec pos) { return materials[pos.x + pos.y * width]; }

  Material* Matp(int x, int y) { return &materials[x + y * width]; }

  unsigned char CheckedPixelWrap(int x, int y) {
    unsigned int idx = static_cast<unsigned int>(x + y * width);
    if (idx < data.size()) return data[idx];
    return 0;
  }

  Material CheckedMatWrap(int x, int y) {
    unsigned int idx = static_cast<unsigned int>(x + y * width);
    if (idx < materials.size()) return materials[idx];
    return zero_material;
  }

  bool Inside(int x, int y) {
    return static_cast<unsigned int>(x) < static_cast<unsigned int>(width) &&
           static_cast<unsigned int>(y) < static_cast<unsigned int>(height);
  }

  bool Inside(fixedvec pos) {
    return static_cast<unsigned int>(pos.x) < static_cast<unsigned int>(width) &&
           static_cast<unsigned int>(pos.y) < static_cast<unsigned int>(height);
  }

  void Swap(Level& other) {
    data.swap(other.data);
    materials.swap(other.materials);
    std::swap(width, other.width);
    std::swap(height, other.height);
    std::swap(origpal, other.origpal);
    std::swap(old_random_level, other.old_random_level);
    std::swap(old_level_file, other.old_level_file);
    std::swap(zero_material, other.zero_material);
  }

  Rect Bounds() { return Rect(0, 0, width, height); }

  void Resize(int width_new, int height_new);

  std::vector<unsigned char> data;
  std::vector<Material> materials;

  bool old_random_level;
  std::string old_level_file;
  int width, height;
  Palette origpal;
  Material zero_material;
};
