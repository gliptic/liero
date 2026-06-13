#include <catch2/catch_test_macros.hpp>

#include "common.hpp"
#include "filesystem.hpp"
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
  CHECK(kLevel.AppearanceAt(5, ColorMode::kClassic, pal32, 0) == 0xFFABCDEF);
}

TEST_CASE("AppearanceAt modern mode, no display layer — still palette path") {
  Common common;
  FillMaterials(common);
  Level const kLevel = MakeClassicLevel(common);

  uint32_t pal32[256] = {};
  pal32[42] = 0xFFABCDEF;

  // Empty display_valid means always fall back, even in modern mode.
  CHECK(kLevel.AppearanceAt(5, ColorMode::kModern, pal32, 0) == 0xFFABCDEF);
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
  CHECK(level.AppearanceAt(5, ColorMode::kModern, pal32, 0) == 0xFF112233);
  // Another pixel with display_valid==0 → palette fallback.
  pal32[0] = 0xFF000001;
  CHECK(level.AppearanceAt(0, ColorMode::kModern, pal32, 0) == 0xFF000001);
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
  CHECK(level.AppearanceAt(5, ColorMode::kClassic, pal32, 0) == 0xFFABCDEF);
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
// Does NOT write ramp_count; produces a display-only block for compat tests.
static void AppendModernBlock(std::vector<uint8_t>& buf, std::vector<uint32_t> const& dd,
                              std::vector<uint8_t> const& dv) {
  static constexpr uint8_t kMagic[8] = {'M', 'O', 'D', 'E', 'R', 'N', 'L', 'V'};
  buf.insert(buf.end(), kMagic, kMagic + 8);
  auto const* raw = reinterpret_cast<uint8_t const*>(dd.data());
  buf.insert(buf.end(), raw, raw + dd.size() * 4);
  buf.insert(buf.end(), dv.begin(), dv.end());
}

// Append ramp table + display_anim after the display_valid data.
// Pass empty ramps to write ramp_count=0 (no anim layer).
static void AppendRampData(std::vector<uint8_t>& buf, std::vector<Level::ArgbRamp> const& ramps,
                           std::vector<uint8_t> const& display_anim) {
  buf.push_back(static_cast<uint8_t>(ramps.size()));
  for (Level::ArgbRamp const& r : ramps) {
    buf.push_back(r.shift);
    auto count = static_cast<uint16_t>(r.colors.size());
    buf.push_back(count & 0xFFU);
    buf.push_back((count >> 8) & 0xFFU);
    auto const* raw = reinterpret_cast<uint8_t const*>(r.colors.data());
    buf.insert(buf.end(), raw, raw + r.colors.size() * 4);
  }
  if (!ramps.empty()) {
    buf.insert(buf.end(), display_anim.begin(), display_anim.end());
  }
}

TEST_CASE("Level::load classic level leaves display layers empty", "[level][display-layer]") {
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

TEST_CASE("Level::load MODERNLV block populates display layers", "[level][display-layer]") {
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

// Animated terrain (ramp layer) tests.

TEST_CASE("Level has argb_ramps and display_anim empty by default", "[level][anim-layer]") {
  Common common;
  FillMaterials(common);
  Level const kLevel(common);
  CHECK(kLevel.argb_ramps.empty());
  CHECK(kLevel.display_anim.empty());
}

TEST_CASE("Level::Swap also swaps argb_ramps and display_anim", "[level][anim-layer]") {
  Common common;
  FillMaterials(common);
  Level a = MakeClassicLevel(common);
  Level::ArgbRamp ramp;
  ramp.colors = {0xFFAA0000U, 0xFF00BB00U};
  ramp.shift = 0;
  a.argb_ramps.push_back(ramp);
  a.display_anim.assign(16, 1);

  Level b(common);
  b.width = 2;
  b.height = 2;
  b.material_id.assign(4, 0);
  b.materials.assign(4, common.materials[0]);

  a.Swap(b);

  CHECK(a.argb_ramps.empty());
  CHECK(a.display_anim.empty());
  CHECK(b.argb_ramps.size() == 1);
  CHECK(b.display_anim.size() == 16);
}

TEST_CASE("AppearanceAt animated pixel cycles through colors as cycles advances",
          "[level][anim-layer]") {
  Common common;
  FillMaterials(common);
  Level level = MakeClassicLevel(common);
  level.display_data.assign(16, 0);
  level.display_valid.assign(16, 1);
  level.display_anim.assign(16, 1);

  Level::ArgbRamp ramp;
  ramp.colors = {0xFFAA0000U, 0xFF00BB00U, 0xFF0000CCU};
  ramp.shift = 0;
  level.argb_ramps.push_back(ramp);

  uint32_t pal32[256] = {};

  CHECK(level.AppearanceAt(5, ColorMode::kModern, pal32, 0) == 0xFFAA0000U);
  CHECK(level.AppearanceAt(5, ColorMode::kModern, pal32, 1) == 0xFF00BB00U);
  CHECK(level.AppearanceAt(5, ColorMode::kModern, pal32, 2) == 0xFF0000CCU);
  CHECK(level.AppearanceAt(5, ColorMode::kModern, pal32, 3) == 0xFFAA0000U);

  // Classic mode never consults ramps — falls back to palette.
  // material_id[5] = 42 (from MakeClassicLevel), so result is pal32[42].
  pal32[42] = 0xFF112233U;
  CHECK(level.AppearanceAt(5, ColorMode::kClassic, pal32, 1) == 0xFF112233U);
}

TEST_CASE("AppearanceAt shift controls cycle speed", "[level][anim-layer]") {
  Common common;
  FillMaterials(common);
  Level level = MakeClassicLevel(common);
  level.display_data.assign(16, 0);
  level.display_valid.assign(16, 1);
  level.display_anim.assign(16, 1);

  Level::ArgbRamp ramp;
  ramp.colors = {0xFFAA0000U, 0xFF00BB00U};
  ramp.shift = 1;
  level.argb_ramps.push_back(ramp);

  uint32_t pal32[256] = {};
  CHECK(level.AppearanceAt(5, ColorMode::kModern, pal32, 0) == 0xFFAA0000U);
  CHECK(level.AppearanceAt(5, ColorMode::kModern, pal32, 1) == 0xFFAA0000U);
  CHECK(level.AppearanceAt(5, ColorMode::kModern, pal32, 2) == 0xFF00BB00U);
  CHECK(level.AppearanceAt(5, ColorMode::kModern, pal32, 3) == 0xFF00BB00U);
}

TEST_CASE("AppearanceAt phase offset in display_data shifts the cycle start",
          "[level][anim-layer]") {
  Common common;
  FillMaterials(common);
  Level level = MakeClassicLevel(common);
  level.display_data.assign(16, 0);
  level.display_valid.assign(16, 1);
  level.display_anim.assign(16, 1);
  level.display_data[5] = 1;  // pixel 5 has phase offset 1

  Level::ArgbRamp ramp;
  ramp.colors = {0xFFAA0000U, 0xFF00BB00U, 0xFF0000CCU};
  ramp.shift = 0;
  level.argb_ramps.push_back(ramp);

  uint32_t pal32[256] = {};
  CHECK(level.AppearanceAt(5, ColorMode::kModern, pal32, 0) == 0xFF00BB00U);  // (1+0)%3=1
  CHECK(level.AppearanceAt(5, ColorMode::kModern, pal32, 1) == 0xFF0000CCU);  // (1+1)%3=2
  CHECK(level.AppearanceAt(5, ColorMode::kModern, pal32, 2) == 0xFFAA0000U);  // (1+2)%3=0
}

TEST_CASE("AppearanceAt out-of-range display_anim falls back to display_data",
          "[level][anim-layer]") {
  Common common;
  FillMaterials(common);
  Level level = MakeClassicLevel(common);
  level.display_data.assign(16, 0);
  level.display_valid.assign(16, 1);
  level.display_anim.assign(16, 0);

  // display_anim[5] = 0 → static authored pixel
  level.display_data[5] = 0xFF112233U;
  uint32_t pal32[256] = {};
  CHECK(level.AppearanceAt(5, ColorMode::kModern, pal32, 0) == 0xFF112233U);

  // display_anim[7] = 5 but only 1 ramp → OOB → fallback to display_data[7]
  Level::ArgbRamp oob_ramp;
  oob_ramp.colors = {0xFFAABBCCU};
  oob_ramp.shift = 0;
  level.argb_ramps.push_back(oob_ramp);
  level.display_anim[7] = 5;
  level.display_data[7] = 0xFF445566U;
  CHECK(level.AppearanceAt(7, ColorMode::kModern, pal32, 0) == 0xFF445566U);
}

TEST_CASE("AppearanceAt no-ramps path matches static display behavior", "[level][anim-layer]") {
  Common common;
  FillMaterials(common);
  Level level = MakeClassicLevel(common);
  level.display_data.assign(16, 0);
  level.display_valid.assign(16, 0);
  level.display_data[5] = 0xFF112233U;
  level.display_valid[5] = 1;

  uint32_t pal32[256] = {};
  pal32[42] = 0xFFABCDEFU;

  // With no ramps (display_anim empty), cycles arg makes no difference.
  CHECK(level.AppearanceAt(5, ColorMode::kModern, pal32, 0) == 0xFF112233U);
  CHECK(level.AppearanceAt(5, ColorMode::kModern, pal32, 42) == 0xFF112233U);
  CHECK(level.AppearanceAt(5, ColorMode::kClassic, pal32, 42) == 0xFFABCDEFU);
}

TEST_CASE("Bitmap cycles field defaults to 0", "[level][anim-layer]") {
  Bitmap bmp;  // NOLINT(misc-const-correctness)
  CHECK(bmp.cycles == 0);
}

TEST_CASE("Bitmap::Copy propagates cycles", "[level][anim-layer]") {
  Bitmap src;
  src.Alloc(4, 4);
  src.cycles = 99;
  Bitmap dest;
  dest.Copy(src);
  CHECK(dest.cycles == 99);
}

TEST_CASE("Level::load POWERLEVEL then MODERNLV block", "[level][display-layer]") {
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

// MODERNLV animation extension loader tests.

TEST_CASE("Level::load MODERNLV with ramp_count=0 leaves anim layer empty", "[level][anim-layer]") {
  Common common;
  FillMaterials(common);
  Settings settings;
  settings.load_powerlevel_palette = false;

  std::vector<uint32_t> dd(kLevCells, 0);
  std::vector<uint8_t> dv(kLevCells, 0);
  dd[0] = 0xFF112233U;
  dv[0] = 1;

  std::vector<uint8_t> bytes(kLevCells, 0);
  AppendModernBlock(bytes, dd, dv);
  AppendRampData(bytes, {}, {});  // ramp_count=0

  io::MemReader r(bytes);
  Level level(common);
  REQUIRE(level.load(common, settings, r));
  REQUIRE(level.display_data.size() == kLevCells);
  CHECK(level.display_data[0] == 0xFF112233U);
  CHECK(level.display_valid[0] == 1);
  CHECK(level.argb_ramps.empty());
  CHECK(level.display_anim.empty());
}

TEST_CASE("Level::load MODERNLV with ramps populates argb_ramps and display_anim",
          "[level][anim-layer]") {
  Common common;
  FillMaterials(common);
  Settings settings;
  settings.load_powerlevel_palette = false;

  // All pixels phase-offset 0; pixel 0 is animated (ramp 1), rest static.
  std::vector<uint32_t> dd(kLevCells, 0);
  std::vector<uint8_t> dv(kLevCells, 0);
  dv[0] = 1;  // pixel 0 authored
  dd[0] = 0;  // phase offset 0

  std::vector<uint8_t> danim(kLevCells, 0);
  danim[0] = 1;  // pixel 0 uses ramp 1

  Level::ArgbRamp ramp;
  ramp.colors = {0xFFAA0000U, 0xFF00BB00U};
  ramp.shift = 1;

  std::vector<uint8_t> bytes(kLevCells, 0);
  AppendModernBlock(bytes, dd, dv);
  AppendRampData(bytes, {ramp}, danim);

  io::MemReader r(bytes);
  Level level(common);
  REQUIRE(level.load(common, settings, r));
  REQUIRE(level.argb_ramps.size() == 1);
  CHECK(level.argb_ramps[0].shift == 1);
  REQUIRE(level.argb_ramps[0].colors.size() == 2);
  CHECK(level.argb_ramps[0].colors[0] == 0xFFAA0000U);
  CHECK(level.argb_ramps[0].colors[1] == 0xFF00BB00U);
  REQUIRE(level.display_anim.size() == kLevCells);
  CHECK(level.display_anim[0] == 1);
  CHECK(level.display_anim[1] == 0);
  // Animated pixel resolves correctly.
  uint32_t pal32[256] = {};
  CHECK(level.AppearanceAt(0, ColorMode::kModern, pal32, 2) == 0xFF00BB00U);
}

TEST_CASE("Level::load MODERNLV truncated ramp data drops anim layer", "[level][anim-layer]") {
  Common common;
  FillMaterials(common);
  Settings settings;
  settings.load_powerlevel_palette = false;

  std::vector<uint32_t> const kDd(kLevCells, 0);
  std::vector<uint8_t> const kDv(kLevCells, 0);
  std::vector<uint8_t> bytes(kLevCells, 0);
  AppendModernBlock(bytes, kDd, kDv);

  // Write ramp_count=1 but truncate the ramp body.
  bytes.push_back(1);  // ramp_count=1
  bytes.push_back(0);  // shift=0
  // color_count=2 but only write 1 byte of the count (truncated).
  bytes.push_back(2);
  // No color data at all — stream ends here.

  io::MemReader r(bytes);
  Level level(common);
  REQUIRE(level.load(common, settings, r));
  // Truncated ramp data → anim layer dropped.
  CHECK(level.argb_ramps.empty());
  CHECK(level.display_anim.empty());
  // Display layer is still valid.
  REQUIRE(level.display_data.size() == kLevCells);
}

TEST_CASE("Level::load MODERNLV zero-length ramp drops anim layer", "[level][anim-layer]") {
  Common common;
  FillMaterials(common);
  Settings settings;
  settings.load_powerlevel_palette = false;

  std::vector<uint32_t> const kDd(kLevCells, 0);
  std::vector<uint8_t> const kDv(kLevCells, 0);
  std::vector<uint8_t> bytes(kLevCells, 0);
  AppendModernBlock(bytes, kDd, kDv);

  bytes.push_back(1);  // ramp_count=1
  bytes.push_back(0);  // shift=0
  bytes.push_back(0);  // color_count low byte = 0
  bytes.push_back(0);  // color_count high byte = 0 → zero-length ramp (invalid)

  io::MemReader r(bytes);
  Level level(common);
  REQUIRE(level.load(common, settings, r));
  CHECK(level.argb_ramps.empty());
  CHECK(level.display_anim.empty());
}

TEST_CASE("Level::load MODERNLV display_anim OOB value drops anim layer", "[level][anim-layer]") {
  Common common;
  FillMaterials(common);
  Settings settings;
  settings.load_powerlevel_palette = false;

  std::vector<uint32_t> const kDd(kLevCells, 0);
  std::vector<uint8_t> const kDv(kLevCells, 0);
  std::vector<uint8_t> danim(kLevCells, 0);
  danim[0] = 2;  // ramp index 2 — but only 1 ramp will be in the table

  Level::ArgbRamp ramp;
  ramp.colors = {0xFFAA0000U};
  ramp.shift = 0;

  std::vector<uint8_t> bytes(kLevCells, 0);
  AppendModernBlock(bytes, kDd, kDv);
  AppendRampData(bytes, {ramp}, danim);

  io::MemReader r(bytes);
  Level level(common);
  REQUIRE(level.load(common, settings, r));
  // display_anim[0]=2 but only 1 ramp → OOB → whole anim layer dropped.
  CHECK(level.argb_ramps.empty());
  CHECK(level.display_anim.empty());
}

// File-based round-trip: modern_test.lev carries one animated ramp.
// The ramp and animated band are written by tools/gen_stage4_anim.py.
// A full-width band of kBandH rows at the top is animated (display_anim==1).
// Phase offset = (x + y) % 4 for a diagonal shimmer.
// Ramp: shift=1, colors=[0xFF1A3A6A, 0xFF2A4A7A, 0xFF3A5A8A, 0xFF0A2A5A].
TEST_CASE("modern_test.lev has anim layer and AppearanceAt cycles correctly",
          "[level][anim-layer][file]") {
  Common common;
  FillMaterials(common);
  Settings settings;
  settings.load_powerlevel_palette = false;

  auto r_ptr = FsNode("data/TC/openliero/Levels/modern_test.lev").ToReader();
  io::Reader& r = *r_ptr;
  Level level(common);
  REQUIRE(level.load(common, settings, r));

  REQUIRE(level.argb_ramps.size() == 1);
  CHECK(level.argb_ramps[0].shift == 1);
  REQUIRE(level.argb_ramps[0].colors.size() == 4);
  CHECK(level.argb_ramps[0].colors[0] == 0xFF1A3A6AU);
  CHECK(level.argb_ramps[0].colors[1] == 0xFF2A4A7AU);
  CHECK(level.argb_ramps[0].colors[2] == 0xFF3A5A8AU);
  CHECK(level.argb_ramps[0].colors[3] == 0xFF0A2A5AU);
  REQUIRE(level.display_anim.size() == kLevCells);

  // Spot-check the animated band: first row, first pixel of row 1, last band row.
  static constexpr int kBandH = 20;
  CHECK(level.display_valid[0] == 1);
  CHECK(level.display_anim[0] == 1);
  CHECK(level.display_valid[kLevW] == 1);
  CHECK(level.display_anim[kLevW] == 1);
  CHECK(level.display_valid[(kBandH - 1) * kLevW] == 1);
  CHECK(level.display_anim[(kBandH - 1) * kLevW] == 1);
  // First pixel past the band is not animated.
  CHECK(level.display_anim[kBandH * kLevW] == 0);

  // Phase offsets: pixel(x,y) has phase=(x+y)%4.
  // Pixel 0 (x=0,y=0): phase=0; pixel 1 (x=1,y=0): phase=1.
  // At cycles=0 (shift=1 → 0>>1=0):
  uint32_t pal32[256] = {};
  CHECK(level.AppearanceAt(0, ColorMode::kModern, pal32, 0) == 0xFF1A3A6AU);
  CHECK(level.AppearanceAt(1, ColorMode::kModern, pal32, 0) == 0xFF2A4A7AU);
  // At cycles=2 (2>>1=1): pixel 0 → colors[(0+1)%4]=colors[1]
  CHECK(level.AppearanceAt(0, ColorMode::kModern, pal32, 2) == 0xFF2A4A7AU);
  CHECK(level.AppearanceAt(1, ColorMode::kModern, pal32, 2) == 0xFF3A5A8AU);
  // Pixel kLevW (x=0,y=1): phase=1; at cycles=0 → colors[1]
  CHECK(level.AppearanceAt(kLevW, ColorMode::kModern, pal32, 0) == 0xFF2A4A7AU);
}
