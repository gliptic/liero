#pragma once

#include <cstdint>
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
}  // namespace io

struct Game;
struct Settings;
struct Rand;
struct Common;
struct ShadowQuery;

struct Level {
  Level(Common& common) : zero_material(common.materials[0]) {}

  bool load(Common& common, Settings const& settings, io::Reader& r);

  void GenerateDirtPattern(Common& common, Rand& rand);
  void GenerateRandom(Common& common, Settings const& settings, Rand& rand);
  void MakeShadow(Common& common);
  void GenerateFromSettings(Common& common, Settings const& settings, Rand& rand);
  bool SelectSpawn(Rand& rand, int w, int h, IVec2& selected);
  void DrawMiniature(Bitmap& dest, int map_x, int map_y, int step_x, int step_y) const;
  void DrawMiniature(Bitmap& dest, int map_x, int map_y, int step) const {
    DrawMiniature(dest, map_x, map_y, step, step);
  }

  // Per-level animation ramp: a short list of ARGB colours that cycle each
  // frame. `shift` controls speed: phase = (cycles >> shift) % colors.size().
  struct ArgbRamp {
    std::vector<uint32_t> colors;
    uint8_t shift{0};
  };

  // What the renderer shows for level pixel `idx`. In modern mode, returns
  // the authored or animated colour; otherwise the palette-derived colour.
  // `cycles` is the simulation frame counter (from Bitmap::cycles, 0 for
  // menu previews). Classic mode and empty display layers always use the
  // palette path.
  uint32_t AppearanceAt(int idx, ColorMode mode, uint32_t const* pal32, int cycles) const {
    if (mode == ColorMode::kModern && !display_valid.empty() && display_valid[idx]) {
      return ResolveDisplayAt(idx, cycles);
    }
    return pal32[material_id[idx]];
  }

  unsigned char Pixel(int x, int y) { return material_id[x + y * width]; }

  unsigned char Pixel(fixedvec pos) { return material_id[pos.x + pos.y * width]; }

  unsigned char* Pixelp(int x, int y) { return &material_id[x + y * width]; }

  void SetPixel(int x, int y, PalIdx w, Common& common) {
    int const kIdx = x + y * width;
    material_id[kIdx] = w;
    materials[kIdx] = common.materials[w];
    if (!display_valid.empty()) {
      display_valid[kIdx] = 0;
    }
  }

  void SetPixel(fixedvec pos, PalIdx w, Common& common) {
    int const kIdx = pos.x + pos.y * width;
    material_id[kIdx] = w;
    materials[kIdx] = common.materials[w];
    if (!display_valid.empty()) {
      display_valid[kIdx] = 0;
    }
  }

  Material& Mat(int x, int y) { return materials[x + y * width]; }

  Material& Mat(fixedvec pos) { return materials[pos.x + pos.y * width]; }

  Material* Matp(int x, int y) { return &materials[x + y * width]; }

  unsigned char CheckedPixelWrap(int x, int y) {
    auto const kIdx = static_cast<unsigned int>(x + y * width);
    if (kIdx < material_id.size()) {
      return material_id[kIdx];
    }
    return 0;
  }

  Material CheckedMatWrap(int x, int y) {
    auto const kIdx = static_cast<unsigned int>(x + y * width);
    if (kIdx < materials.size()) {
      return materials[kIdx];
    }
    return zero_material;
  }

  bool Inside(int x, int y) const {
    return static_cast<unsigned int>(x) < static_cast<unsigned int>(width) &&
           static_cast<unsigned int>(y) < static_cast<unsigned int>(height);
  }

  bool Inside(fixedvec pos) const {
    return static_cast<unsigned int>(pos.x) < static_cast<unsigned int>(width) &&
           static_cast<unsigned int>(pos.y) < static_cast<unsigned int>(height);
  }

  void Swap(Level& other) {
    material_id.swap(other.material_id);
    materials.swap(other.materials);
    display_data.swap(other.display_data);
    display_valid.swap(other.display_valid);
    argb_ramps.swap(other.argb_ramps);
    display_anim.swap(other.display_anim);
    std::swap(width, other.width);
    std::swap(height, other.height);
    std::swap(origpal, other.origpal);
    std::swap(has_custom_palette, other.has_custom_palette);
    std::swap(old_random_level, other.old_random_level);
    std::swap(old_level_file, other.old_level_file);
    std::swap(old_random_map_width, other.old_random_map_width);
    std::swap(old_random_map_height, other.old_random_map_height);
    std::swap(zero_material, other.zero_material);
  }

  Rect Bounds() const { return {0, 0, width, height}; }

  // Levels received over the wire or from a replay don't carry the
  // custom-palette flag; derive it by comparing against the stock palette.
  // A custom palette that happens to equal the stock one is visually
  // indistinguishable either way.
  void DeriveHasCustomPalette(Palette const& stock) {
    has_custom_palette = false;
    for (int i = 0; i < 256; ++i) {
      if (origpal.entries[i].r != stock.entries[i].r ||
          origpal.entries[i].g != stock.entries[i].g ||
          origpal.entries[i].b != stock.entries[i].b) {
        has_custom_palette = true;
        return;
      }
    }
  }

  void Resize(int width_new, int height_new);

  std::vector<unsigned char> material_id;
  std::vector<Material> materials;
  // Optional true-colour display layer (modern levels only). Both stay empty
  // for classic levels — empty means "always use the palette path."
  std::vector<uint32_t> display_data;
  std::vector<uint8_t> display_valid;
  // Optional animation layer. Empty when the level has no ramps.
  // argb_ramps: the ramp table; display_anim[idx]: 0=static, N=ramp N-1.
  // For animated pixels, display_data[idx] is a per-pixel phase offset, not
  // a colour. All three fields are immutable after load; never snapshotted.
  std::vector<ArgbRamp> argb_ramps;
  std::vector<uint8_t> display_anim;

  bool old_random_level;
  std::string old_level_file;
  int32_t old_random_map_width{504};
  int32_t old_random_map_height{350};
  int width{0}, height{0};
  Palette origpal;
  // True when the level shipped its own palette (e.g. POWERLEVEL); such a
  // palette wins in both colour modes. Not serialized: netplay- and
  // replay-received levels re-derive it (DeriveHasCustomPalette).
  bool has_custom_palette = false;
  Material zero_material;

 private:
  friend struct ShadowQuery;

  // Resolves the modern-authored colour at `idx` (caller must ensure
  // display_valid[idx] is true). Returns the animated colour when ramps are
  // active, otherwise the static display_data value.
  uint32_t ResolveDisplayAt(int idx, int cycles) const {
    uint8_t const kA = display_anim.empty() ? 0 : display_anim[idx];
    if (kA == 0 || static_cast<std::size_t>(kA) > argb_ramps.size()) {
      return display_data[idx];
    }
    ArgbRamp const& r = argb_ramps[kA - 1];
    if (r.colors.empty()) {
      return display_data[idx];
    }
    // `shift` comes from level data (file/wire/snapshot); a value >= 32 would
    // be undefined behaviour in the shift below (and platform-divergent), so
    // treat it as a frozen animation (advance only by the per-pixel offset).
    unsigned const kInc = r.shift < 32 ? (static_cast<unsigned>(cycles) >> r.shift) : 0U;
    unsigned const kPhase = display_data[idx] + kInc;
    return r.colors[kPhase % r.colors.size()];
  }
};
