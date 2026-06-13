#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>
#include <vector>

#include "common.hpp"
#include "io/stream.hpp"
#include "level.hpp"
#include "settings.hpp"

// The sized level format header:
//   magic[8]   = "OLLEVEL2"
//   version[1] = 0
//   width[2]   = little-endian uint16_t
//   height[2]  = little-endian uint16_t
// followed by the existing body (material bytes → optional POWERLEVEL → optional MODERNLV).

static constexpr uint8_t kMagic[8] = {'O', 'L', 'L', 'E', 'V', 'E', 'L', '2'};
static constexpr int kLegacyW = 504;
static constexpr int kLegacyH = 350;
static constexpr std::size_t kLegacyCells = kLegacyW * kLegacyH;

static void FillMaterials(Common& common) {
  for (auto& mat : common.materials) {
    mat.flags = 0;
  }
}

// Build a minimal sized-format byte buffer: OLLEVEL2 header + w*h material bytes.
static std::vector<uint8_t> MakeSizedBuf(int w, int h, uint8_t fill = 0) {
  std::vector<uint8_t> buf;
  buf.insert(buf.end(), kMagic, kMagic + 8);
  buf.push_back(0);  // version
  buf.push_back(static_cast<uint8_t>(w & 0xFF));
  buf.push_back(static_cast<uint8_t>((w >> 8) & 0xFF));
  buf.push_back(static_cast<uint8_t>(h & 0xFF));
  buf.push_back(static_cast<uint8_t>((h >> 8) & 0xFF));
  buf.insert(buf.end(), static_cast<std::size_t>(w) * static_cast<std::size_t>(h), fill);
  return buf;
}

// Append MODERNLV block to buf for a level of w*h cells.
// Writes ramp_count=0 to match the production format from lev_gen.py.
static void AppendModernBlock(std::vector<uint8_t>& buf, int w, int h,
                              std::vector<uint32_t> const& dd, std::vector<uint8_t> const& dv) {
  static constexpr uint8_t kMod[8] = {'M', 'O', 'D', 'E', 'R', 'N', 'L', 'V'};
  buf.insert(buf.end(), kMod, kMod + 8);
  std::size_t const kCells = static_cast<std::size_t>(w) * static_cast<std::size_t>(h);
  auto const* raw = reinterpret_cast<uint8_t const*>(dd.data());
  buf.insert(buf.end(), raw, raw + kCells * 4);
  buf.insert(buf.end(), dv.begin(), dv.end());
  buf.push_back(0);  // ramp_count = 0
}

// ---------------------------------------------------------------------------
// Legacy (no-header) format regression
// ---------------------------------------------------------------------------

TEST_CASE("Level::load legacy format still loads at 504x350", "[sized-level]") {
  Common common;
  FillMaterials(common);
  Settings settings;
  settings.load_powerlevel_palette = false;

  std::vector<uint8_t> const kBytes(kLegacyCells, 0);
  io::MemReader r(kBytes);

  Level level(common);
  REQUIRE(level.load(common, settings, r));
  CHECK(level.width == kLegacyW);
  CHECK(level.height == kLegacyH);
  CHECK(level.material_id.size() == kLegacyCells);
}

TEST_CASE("Level::load legacy format material bytes survive the header probe", "[sized-level]") {
  Common common;
  FillMaterials(common);
  Settings settings;
  settings.load_powerlevel_palette = false;

  // First 8 bytes not matching OLLEVEL2 magic; set to known material values.
  std::vector<uint8_t> bytes(kLegacyCells, 42);
  bytes[0] = 19;  // rock — non-zero, non-'O'
  bytes[7] = 12;  // dirt
  io::MemReader r(bytes);

  Level level(common);
  REQUIRE(level.load(common, settings, r));
  CHECK(level.width == kLegacyW);
  CHECK(level.height == kLegacyH);
  CHECK(level.material_id[0] == 19);
  CHECK(level.material_id[7] == 12);
  CHECK(level.material_id[8] == 42);
}

// ---------------------------------------------------------------------------
// Sized format: basic loading
// ---------------------------------------------------------------------------

TEST_CASE("Level::load OLLEVEL2 header drives Resize to specified size", "[sized-level]") {
  Common common;
  FillMaterials(common);
  Settings settings;
  settings.load_powerlevel_palette = false;

  auto const kBuf = MakeSizedBuf(100, 80);
  io::MemReader r(kBuf);

  Level level(common);
  REQUIRE(level.load(common, settings, r));
  CHECK(level.width == 100);
  CHECK(level.height == 80);
  CHECK(level.material_id.size() == 100U * 80U);
}

TEST_CASE("Level::load OLLEVEL2 material bytes are read correctly", "[sized-level]") {
  Common common;
  FillMaterials(common);
  Settings settings;
  settings.load_powerlevel_palette = false;

  std::vector<uint8_t> buf = MakeSizedBuf(10, 8, 0);
  // Material at (3,2) = index 19 (rock). Header is 13 bytes.
  buf[13 + 2 * 10 + 3] = 19;

  io::MemReader r(buf);
  Level level(common);
  REQUIRE(level.load(common, settings, r));
  REQUIRE(level.width == 10);
  REQUIRE(level.height == 8);
  CHECK(level.material_id[2 * 10 + 3] == 19);
}

// ---------------------------------------------------------------------------
// Sized format: cap and minimum enforcement
// ---------------------------------------------------------------------------

TEST_CASE("Level::load OLLEVEL2 rejects width > 4096", "[sized-level]") {
  Common common;
  FillMaterials(common);
  Settings settings;
  settings.load_powerlevel_palette = false;

  // Build only the header (no body) — load should reject before reading material.
  std::vector<uint8_t> buf;
  buf.insert(buf.end(), kMagic, kMagic + 8);
  buf.push_back(0);  // version
  buf.push_back(0x01);
  buf.push_back(0x10);  // width  = 4097 (0x1001 LE)
  buf.push_back(0x01);
  buf.push_back(0x00);  // height = 1
  io::MemReader r(buf);

  Level level(common);
  CHECK_FALSE(level.load(common, settings, r));
}

TEST_CASE("Level::load OLLEVEL2 rejects height > 4096", "[sized-level]") {
  Common common;
  FillMaterials(common);
  Settings settings;
  settings.load_powerlevel_palette = false;

  std::vector<uint8_t> buf;
  buf.insert(buf.end(), kMagic, kMagic + 8);
  buf.push_back(0);  // version
  buf.push_back(0x01);
  buf.push_back(0x00);  // width  = 1
  buf.push_back(0x01);
  buf.push_back(0x10);  // height = 4097
  io::MemReader r(buf);

  Level level(common);
  CHECK_FALSE(level.load(common, settings, r));
}

TEST_CASE("Level::load OLLEVEL2 rejects width == 0", "[sized-level]") {
  Common common;
  FillMaterials(common);
  Settings settings;
  settings.load_powerlevel_palette = false;

  std::vector<uint8_t> buf;
  buf.insert(buf.end(), kMagic, kMagic + 8);
  buf.push_back(0);  // version
  buf.push_back(0x00);
  buf.push_back(0x00);  // width  = 0
  buf.push_back(0x01);
  buf.push_back(0x00);  // height = 1
  io::MemReader r(buf);

  Level level(common);
  CHECK_FALSE(level.load(common, settings, r));
}

TEST_CASE("Level::load OLLEVEL2 rejects height == 0", "[sized-level]") {
  Common common;
  FillMaterials(common);
  Settings settings;
  settings.load_powerlevel_palette = false;

  std::vector<uint8_t> buf;
  buf.insert(buf.end(), kMagic, kMagic + 8);
  buf.push_back(0);  // version
  buf.push_back(0x01);
  buf.push_back(0x00);  // width  = 1
  buf.push_back(0x00);
  buf.push_back(0x00);  // height = 0
  io::MemReader r(buf);

  Level level(common);
  CHECK_FALSE(level.load(common, settings, r));
}

TEST_CASE("Level::load OLLEVEL2 accepts 4096x4096", "[sized-level]") {
  Common common;
  FillMaterials(common);
  Settings settings;
  settings.load_powerlevel_palette = false;

  auto const kBuf = MakeSizedBuf(4096, 4096, 0);
  io::MemReader r(kBuf);

  Level level(common);
  REQUIRE(level.load(common, settings, r));
  CHECK(level.width == 4096);
  CHECK(level.height == 4096);
}

// ---------------------------------------------------------------------------
// Sized format: MODERNLV extension
// ---------------------------------------------------------------------------

TEST_CASE("Level::load OLLEVEL2 + MODERNLV block at non-standard size", "[sized-level]") {
  Common common;
  FillMaterials(common);
  Settings settings;
  settings.load_powerlevel_palette = false;

  constexpr int kW = 20;
  constexpr int kH = 15;
  constexpr std::size_t kCells = static_cast<std::size_t>(kW) * kH;

  std::vector<uint32_t> dd(kCells, 0);
  std::vector<uint8_t> dv(kCells, 0);
  dd[3] = 0xFF112233U;
  dv[3] = 1;

  auto buf = MakeSizedBuf(kW, kH, 0);
  AppendModernBlock(buf, kW, kH, dd, dv);

  io::MemReader r(buf);
  Level level(common);
  REQUIRE(level.load(common, settings, r));
  REQUIRE(level.width == kW);
  REQUIRE(level.height == kH);
  REQUIRE(level.display_data.size() == kCells);
  REQUIRE(level.display_valid.size() == kCells);
  CHECK(level.display_data[3] == 0xFF112233U);
  CHECK(level.display_valid[3] == 1);
  CHECK(level.display_valid[0] == 0);
}

// ---------------------------------------------------------------------------
// Sized format: POWERLEVEL + MODERNLV
// ---------------------------------------------------------------------------

TEST_CASE("Level::load OLLEVEL2 + POWERLEVEL + MODERNLV", "[sized-level]") {
  Common common;
  FillMaterials(common);
  Settings settings;
  settings.load_powerlevel_palette = true;

  constexpr int kW = 12;
  constexpr int kH = 10;
  constexpr std::size_t kCells = static_cast<std::size_t>(kW) * kH;

  auto buf = MakeSizedBuf(kW, kH, 0);

  // POWERLEVEL block
  static constexpr uint8_t kPL[10] = {'P', 'O', 'W', 'E', 'R', 'L', 'E', 'V', 'E', 'L'};
  buf.insert(buf.end(), kPL, kPL + 10);
  buf.insert(buf.end(), 768, 0);  // palette all-zero

  // MODERNLV block
  std::vector<uint32_t> dd(kCells, 0);
  std::vector<uint8_t> dv(kCells, 0);
  dd[7] = 0xFFAABBCCU;
  dv[7] = 1;
  AppendModernBlock(buf, kW, kH, dd, dv);

  io::MemReader r(buf);
  Level level(common);
  REQUIRE(level.load(common, settings, r));
  CHECK(level.width == kW);
  CHECK(level.height == kH);
  CHECK(level.has_custom_palette);
  REQUIRE(level.display_data.size() == kCells);
  CHECK(level.display_data[7] == 0xFFAABBCCU);
  CHECK(level.display_valid[7] == 1);
}

// ---------------------------------------------------------------------------
// Sized format: boundary cases
// ---------------------------------------------------------------------------

TEST_CASE("Level::load OLLEVEL2 accepts 1x1 level", "[sized-level]") {
  Common common;
  FillMaterials(common);
  Settings settings;
  settings.load_powerlevel_palette = false;

  std::vector<uint8_t> buf = MakeSizedBuf(1, 1, 0);
  buf[13] = 42;  // single material byte (header is 13 bytes)

  io::MemReader r(buf);
  Level level(common);
  REQUIRE(level.load(common, settings, r));
  CHECK(level.width == 1);
  CHECK(level.height == 1);
  CHECK(level.material_id[0] == 42);
}
