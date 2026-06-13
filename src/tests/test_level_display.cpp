#include <catch2/catch_test_macros.hpp>

#include "common.hpp"
#include "gfx/bitmap.hpp"
#include "gfx/palette.hpp"
#include "io/stream.hpp"
#include "level.hpp"
#include "settings.hpp"

// Minimal Common stub: real Common is heavyweight (loads assets), but the
// display-layer tests only need materials[0..255] populated.
static void FillMaterials(Common& common) {
  for (auto& mat : common.materials) {
    mat.flags = 0;
  }
}

// Build a tiny level with no display layer (classic path).
static Level MakeClassicLevel(Common& common) {
  Level level(common);
  level.width = 4;
  level.height = 4;
  level.material_id.assign(16, 0);
  level.materials.assign(16, common.materials[0]);
  // material_id[5] = palette index 42
  level.material_id[5] = 42;
  level.materials[5] = common.materials[42];
  return level;
}

TEST_CASE("Level::display_data and display_valid are empty for classic levels") {
  Common common;
  FillMaterials(common);
  Level const kLevel = MakeClassicLevel(common);

  CHECK(kLevel.display_data.empty());
  CHECK(kLevel.display_valid.empty());
}

TEST_CASE("AppearanceAt classic mode, no display layer — palette path") {
  Common common;
  FillMaterials(common);
  Level const kLevel = MakeClassicLevel(common);

  uint32_t pal32[256] = {};
  pal32[42] = 0xFFABCDEF;

  // With empty display layer, classic mode returns pal32[material_id[idx]].
  CHECK(kLevel.AppearanceAt(5, ColorMode::kClassic, pal32) == 0xFFABCDEF);
}

TEST_CASE("AppearanceAt modern mode, no display layer — still palette path") {
  Common common;
  FillMaterials(common);
  Level const kLevel = MakeClassicLevel(common);

  uint32_t pal32[256] = {};
  pal32[42] = 0xFFABCDEF;

  // Empty display_valid means always fall back, even in modern mode.
  CHECK(kLevel.AppearanceAt(5, ColorMode::kModern, pal32) == 0xFFABCDEF);
}

TEST_CASE("AppearanceAt modern mode, authored pixel — returns display_data") {
  Common common;
  FillMaterials(common);
  Level level = MakeClassicLevel(common);

  // Install a display layer with pixel 5 authored.
  level.display_data.assign(16, 0);
  level.display_valid.assign(16, 0);
  level.display_data[5] = 0xFF112233;
  level.display_valid[5] = 1;

  uint32_t pal32[256] = {};
  pal32[42] = 0xFFABCDEF;

  // Modern mode + valid → authored ARGB.
  CHECK(level.AppearanceAt(5, ColorMode::kModern, pal32) == 0xFF112233);
  // Another pixel with display_valid==0 → palette fallback.
  pal32[0] = 0xFF000001;
  CHECK(level.AppearanceAt(0, ColorMode::kModern, pal32) == 0xFF000001);
}

TEST_CASE("AppearanceAt classic mode, authored pixel — ignores display_data") {
  Common common;
  FillMaterials(common);
  Level level = MakeClassicLevel(common);

  level.display_data.assign(16, 0);
  level.display_valid.assign(16, 0);
  level.display_data[5] = 0xFF112233;
  level.display_valid[5] = 1;

  uint32_t pal32[256] = {};
  pal32[42] = 0xFFABCDEF;

  // Classic mode ignores display_data entirely.
  CHECK(level.AppearanceAt(5, ColorMode::kClassic, pal32) == 0xFFABCDEF);
}

TEST_CASE("Level::Swap also swaps display layer") {
  Common common;
  FillMaterials(common);

  Level a = MakeClassicLevel(common);
  a.display_data.assign(16, 0xDEADBEEF);
  a.display_valid.assign(16, 1);

  Level b(common);
  b.width = 2;
  b.height = 2;
  b.material_id.assign(4, 0);
  b.materials.assign(4, common.materials[0]);
  // b has no display layer

  a.Swap(b);

  CHECK(a.display_data.empty());
  CHECK(a.display_valid.empty());
  CHECK(b.display_data.size() == 16);
  CHECK(b.display_valid[0] == 1);
}

TEST_CASE("Bitmap has a mode field defaulting to kClassic") {
  Bitmap bmp;  // NOLINT(misc-const-correctness) — const Bitmap triggers -fpermissive
  CHECK(bmp.mode == ColorMode::kClassic);
}

// Task 3: SetPixel must clear display_valid for authored cells.

TEST_CASE("Level::SetPixel clears display_valid for an authored cell") {
  Common common;
  FillMaterials(common);
  Level level = MakeClassicLevel(common);

  level.display_data.assign(16, 0);
  level.display_valid.assign(16, 0);
  level.display_valid[5] = 1;  // pixel (1,1) authored; 4-wide level → idx 5

  level.SetPixel(1, 1, 0, common);

  CHECK(level.display_valid[5] == 0);
  CHECK(level.display_valid[0] == 0);  // untouched pixel unaffected
}

TEST_CASE("Level::SetPixel on classic level (empty display layer) does not crash") {
  Common common;
  FillMaterials(common);
  Level level = MakeClassicLevel(common);

  CHECK_NOTHROW(level.SetPixel(0, 0, 7, common));
  CHECK(level.display_valid.empty());
}

// Task 7: Level::load must detect and read the optional MODERNLV extension.

static constexpr int kLevW = 504, kLevH = 350;
static constexpr std::size_t kLevCells = kLevW * kLevH;

// Append MODERNLV block (magic + display_data + display_valid) to buf.
static void AppendModernBlock(std::vector<uint8_t>& buf, std::vector<uint32_t> const& dd,
                              std::vector<uint8_t> const& dv) {
  static constexpr uint8_t kMagic[8] = {'M', 'O', 'D', 'E', 'R', 'N', 'L', 'V'};
  buf.insert(buf.end(), kMagic, kMagic + 8);
  auto const* raw = reinterpret_cast<uint8_t const*>(dd.data());
  buf.insert(buf.end(), raw, raw + dd.size() * 4);
  buf.insert(buf.end(), dv.begin(), dv.end());
}

TEST_CASE("Level::load classic level leaves display layers empty", "[level][stage3]") {
  Common common;
  FillMaterials(common);
  Settings settings;
  settings.load_powerlevel_palette = false;

  std::vector<uint8_t> const kBytes(kLevCells, 0);
  io::MemReader r(kBytes);

  Level level(common);
  REQUIRE(level.load(common, settings, r));
  CHECK(level.display_data.empty());
  CHECK(level.display_valid.empty());
}

TEST_CASE("Level::load MODERNLV block populates display layers", "[level][stage3]") {
  Common common;
  FillMaterials(common);
  Settings settings;
  settings.load_powerlevel_palette = false;  // skip POWERLEVEL probe

  std::vector<uint32_t> dd(kLevCells, 0);
  std::vector<uint8_t> dv(kLevCells, 0);
  dd[0] = 0xFF112233U;
  dv[0] = 1;
  dd[kLevCells - 1] = 0xFF445566U;
  dv[kLevCells - 1] = 1;

  std::vector<uint8_t> bytes(kLevCells, 0);
  AppendModernBlock(bytes, dd, dv);
  io::MemReader r(bytes);

  Level level(common);
  REQUIRE(level.load(common, settings, r));
  REQUIRE(level.display_data.size() == kLevCells);
  REQUIRE(level.display_valid.size() == kLevCells);
  CHECK(level.display_data[0] == 0xFF112233U);
  CHECK(level.display_valid[0] == 1);
  CHECK(level.display_data[kLevCells - 1] == 0xFF445566U);
  CHECK(level.display_valid[kLevCells - 1] == 1);
  // An unset pixel falls back to palette (display_valid == 0).
  CHECK(level.display_valid[1] == 0);
}

TEST_CASE("Level::load POWERLEVEL then MODERNLV block", "[level][stage3]") {
  Common common;
  FillMaterials(common);
  Settings settings;
  settings.load_powerlevel_palette = true;

  // Build: pixel_data + "POWERLEVEL" + palette(768 zeros) + MODERNLV + display
  std::vector<uint8_t> bytes(kLevCells, 0);

  static constexpr uint8_t kPL[10] = {'P', 'O', 'W', 'E', 'R', 'L', 'E', 'V', 'E', 'L'};
  bytes.insert(bytes.end(), kPL, kPL + 10);
  bytes.insert(bytes.end(), 768, 0);  // palette (all-zero = black)

  std::vector<uint32_t> dd(kLevCells, 0);
  std::vector<uint8_t> dv(kLevCells, 0);
  dd[7] = 0xFFAABBCCU;
  dv[7] = 1;
  AppendModernBlock(bytes, dd, dv);

  io::MemReader r(bytes);

  Level level(common);
  REQUIRE(level.load(common, settings, r));
  CHECK(level.has_custom_palette);
  REQUIRE(level.display_data.size() == kLevCells);
  CHECK(level.display_data[7] == 0xFFAABBCCU);
  CHECK(level.display_valid[7] == 1);
}
