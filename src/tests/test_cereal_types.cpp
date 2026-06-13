// Phase 4 tests: per-type cereal serialize() round-trips through both
// the binary (PortableBinaryArchive) and TOML (cereal::TomlOutputArchive)
// archives. The old archive() functions stay in place; this verifies
// the new cereal-based path before Phase 6 swaps the call sites.

#include <catch2/catch_test_macros.hpp>

#include <cereal/archives/portable_binary.hpp>

#include "game/serialization/cereal_types.hpp"
#include "game/serialization/toml_archive.hpp"

#include <sstream>

namespace {

template <typename T>
T RoundtripBinary(T const& src) {
  std::stringstream ss;
  {
    // Cereal archives mutate state on operator(); they cannot be const.
    cereal::PortableBinaryOutputArchive ar(ss);  // NOLINT(misc-const-correctness)
    ar(cereal::make_nvp("v", src));
  }
  T dst{};
  {
    cereal::PortableBinaryInputArchive ar(ss);  // NOLINT(misc-const-correctness)
    ar(cereal::make_nvp("v", dst));
  }
  return dst;
}

template <typename T>
T RoundtripToml(T const& src) {
  std::stringstream ss;
  {
    cereal::TomlOutputArchive ar(ss);  // NOLINT(misc-const-correctness)
    ar(cereal::make_nvp("v", src));
  }
  T dst{};
  {
    cereal::TomlInputArchive ar(ss);  // NOLINT(misc-const-correctness)
    ar(cereal::make_nvp("v", dst));
  }
  return dst;
}

}  // namespace

TEST_CASE("cereal_types: BasicVec<int,2> binary round-trip", "[cereal_types]") {
  IVec2 const kSrc{-12345, 6789};
  IVec2 const kDst = RoundtripBinary(kSrc);
  CHECK(kDst.x == kSrc.x);
  CHECK(kDst.y == kSrc.y);
}

TEST_CASE("cereal_types: BasicVec<int,2> toml round-trip", "[cereal_types]") {
  IVec2 const kSrc{-12345, 6789};
  IVec2 const kDst = RoundtripToml(kSrc);
  CHECK(kDst.x == kSrc.x);
  CHECK(kDst.y == kSrc.y);
}

TEST_CASE("cereal_types: BasicVec<float,2> binary round-trip", "[cereal_types]") {
  FVec2 const kSrc{1.5F, -2.25F};
  FVec2 const kDst = RoundtripBinary(kSrc);
  CHECK(kDst.x == kSrc.x);
  CHECK(kDst.y == kSrc.y);
}

TEST_CASE("cereal_types: BasicRect<int> binary round-trip", "[cereal_types]") {
  Rect const kSrc{1, 2, 100, 200};
  Rect const kDst = RoundtripBinary(kSrc);
  CHECK(kDst.x1 == 1);
  CHECK(kDst.y1 == 2);
  CHECK(kDst.x2 == 100);
  CHECK(kDst.y2 == 200);
}

TEST_CASE("cereal_types: BasicRect<int> toml round-trip", "[cereal_types]") {
  Rect const kSrc{1, 2, 100, 200};
  Rect const kDst = RoundtripToml(kSrc);
  CHECK(kDst.x1 == 1);
  CHECK(kDst.y1 == 2);
  CHECK(kDst.x2 == 100);
  CHECK(kDst.y2 == 200);
}

TEST_CASE("cereal_types: ControlState round-trip", "[cereal_types]") {
  Worm::ControlState src;
  src.istate = 0x5a;
  Worm::ControlState const kBin = RoundtripBinary(src);
  Worm::ControlState const kToml = RoundtripToml(src);
  CHECK(kBin.istate == src.istate);
  CHECK(kToml.istate == src.istate);
}

TEST_CASE("cereal_types: Ninjarope round-trip excludes anchor", "[cereal_types]") {
  Ninjarope src;
  src.out = true;
  src.attached = true;
  src.pos = fixedvec{1234, -5678};
  src.vel = fixedvec{-1, 2};
  src.length = 700;
  src.cur_len = 350;
  // anchor is intentionally not serialized; verify dst.anchor stays default.
  src.anchor = reinterpret_cast<Worm*>(0xdeadbeef);

  Ninjarope bin = RoundtripBinary(src);
  CHECK(bin.out == true);
  CHECK(bin.attached == true);
  CHECK(bin.pos.x == 1234);
  CHECK(bin.pos.y == -5678);
  CHECK(bin.vel.x == -1);
  CHECK(bin.vel.y == 2);
  CHECK(bin.length == 700);
  CHECK(bin.cur_len == 350);
  CHECK(bin.anchor == nullptr);
}

TEST_CASE("cereal_types: Viewport round-trip", "[cereal_types]") {
  Viewport src;
  src.x = 100;
  src.y = -50;
  src.shake = 7;
  src.max_x = 320;
  src.max_y = 200;
  src.center_x = 160;
  src.center_y = 100;
  src.worm_idx = 1;
  src.banner_y = -8;
  src.rect = Rect{10, 20, 30, 40};

  Viewport const kBin = RoundtripBinary(src);
  CHECK(kBin.x == 100);
  CHECK(kBin.y == -50);
  CHECK(kBin.shake == 7);
  CHECK(kBin.worm_idx == 1);
  CHECK(kBin.rect.x1 == 10);
  CHECK(kBin.rect.y2 == 40);
}

TEST_CASE("cereal_types: Worm round-trip excludes context fields", "[cereal_types]") {
  Worm src;
  src.pos = fixedvec{1 << 16, 2 << 16};
  src.vel = fixedvec{-3, 4};
  src.logic_respawn = IVec2{50, 60};
  src.health = 75;
  src.lives = 3;
  src.kills = 11;
  src.aiming_angle = 90 << 16;
  src.last_killed_by_idx = 2;
  src.current_weapon = 3;
  src.direction = 1;
  src.control_states.istate = 0x42;
  src.weapons[0].ammo = 99;
  src.weapons[2].delay_left = 17;
  src.reacts[1] = 5;
  src.ninjarope.out = true;
  src.ninjarope.length = 500;

  Worm const kBin = RoundtripBinary(src);
  CHECK(kBin.pos.x == src.pos.x);
  CHECK(kBin.pos.y == src.pos.y);
  CHECK(kBin.logic_respawn.x == 50);
  CHECK(kBin.health == 75);
  CHECK(kBin.lives == 3);
  CHECK(kBin.aiming_angle == 90 << 16);
  CHECK(kBin.last_killed_by_idx == 2);
  CHECK(kBin.current_weapon == 3);
  CHECK(kBin.direction == 1);
  CHECK(kBin.control_states.istate == 0x42);
  CHECK(kBin.weapons[0].ammo == 99);
  CHECK(kBin.weapons[2].delay_left == 17);
  CHECK(kBin.reacts[1] == 5);
  CHECK(kBin.ninjarope.out == true);
  CHECK(kBin.ninjarope.length == 500);

  Worm const kToml = RoundtripToml(src);
  CHECK(kToml.pos.x == src.pos.x);
  CHECK(kToml.health == 75);
  CHECK(kToml.weapons[0].ammo == 99);
  CHECK(kToml.ninjarope.length == 500);
}

TEST_CASE("cereal_types: WormSettings round-trip", "[cereal_types]") {
  WormSettings src;
  src.health = 150;
  src.controller = 1;
  src.name = "Hero";
  src.random_name = false;
  src.color = 3;
  src.rgb[0] = 10;
  src.rgb[1] = 20;
  src.rgb[2] = 30;
  src.weapons[0] = 5;
  src.weapons[4] = 7;
  src.controls[WormSettings::kUp] = 100U;

  WormSettings bin = RoundtripBinary(src);
  CHECK(bin.health == 150);
  CHECK(bin.controller == 1);
  CHECK(bin.name == "Hero");
  CHECK(bin.random_name == false);
  CHECK(bin.color == 3);
  CHECK(bin.rgb[0] == 10);
  CHECK(bin.rgb[2] == 30);
  CHECK(bin.weapons[0] == 5);
  CHECK(bin.weapons[4] == 7);
  CHECK(bin.controls[WormSettings::kUp] == 100U);

  WormSettings toml = RoundtripToml(src);
  CHECK(toml.name == "Hero");
  CHECK(toml.weapons[4] == 7);
}

TEST_CASE("cereal_types: Settings round-trip with shared worm settings", "[cereal_types]") {
  Settings src;
  src.max_bonuses = 12;
  src.lives = 5;
  src.shadow = true;
  src.tc = "openliero";
  src.weap_table[0] = 2;
  src.weap_table[39] = 1;
  for (int i = 0; i < Settings::kNumWormSettings; ++i) {
    src.worm_settings[i] = std::make_shared<WormSettings>();
    src.worm_settings[i]->health = 100 + i;
    src.worm_settings[i]->name = "worm" + std::to_string(i);
  }

  Settings bin = RoundtripBinary(src);
  CHECK(bin.max_bonuses == 12);
  CHECK(bin.lives == 5);
  CHECK(bin.shadow == true);
  CHECK(bin.tc == "openliero");
  CHECK(bin.weap_table[0] == 2);
  CHECK(bin.weap_table[39] == 1);
  for (int i = 0; i < Settings::kNumWormSettings; ++i) {
    REQUIRE(bin.worm_settings[i] != nullptr);
    CHECK(bin.worm_settings[i]->health == 100 + i);
    CHECK(bin.worm_settings[i]->name == "worm" + std::to_string(i));
  }
}

TEST_CASE("cereal_types: Color round-trip", "[cereal_types]") {
  Color const kSrc{.r = 10, .g = 20, .b = 30, .unused = 0};
  Color const kBin = RoundtripBinary(kSrc);
  CHECK(kBin.r == 10);
  CHECK(kBin.g == 20);
  CHECK(kBin.b == 30);
}

TEST_CASE("cereal_types: Palette round-trip", "[cereal_types]") {
  Palette src;
  for (int i = 0; i < 256; ++i) {
    src.entries[i].r = static_cast<uint8_t>(i);
    src.entries[i].g = static_cast<uint8_t>(255 - i);
    src.entries[i].b = static_cast<uint8_t>(i ^ 0x55);
  }
  Palette const kBin = RoundtripBinary(src);
  for (int i = 0; i < 256; ++i) {
    CHECK(std::cmp_equal(kBin.entries[i].r, i));
    CHECK(std::cmp_equal(kBin.entries[i].g, 255 - i));
    CHECK(std::cmp_equal(kBin.entries[i].b, i ^ 0x55));
  }
}

TEST_CASE("cereal_types: Level round-trip preserves data and palette", "[cereal_types]") {
  Common common;
  Level src(common);
  src.width = 4;
  src.height = 3;
  src.material_id.assign(src.width * src.height, 0);
  for (int i = 0; i < src.width * src.height; ++i) {
    src.material_id[i] = static_cast<unsigned char>(i * 7 + 1);
  }
  for (int i = 0; i < 256; ++i) {
    src.origpal.entries[i].r = static_cast<uint8_t>(i);
  }

  Level dst(common);
  // Serialize directly without dst pre-construct equivalence to old API.
  std::stringstream ss;
  {
    cereal::PortableBinaryOutputArchive ar(ss);
    ar(cereal::make_nvp("lvl", src));
  }
  {
    cereal::PortableBinaryInputArchive ar(ss);
    ar(cereal::make_nvp("lvl", dst));
  }
  CHECK(dst.width == 4);
  CHECK(dst.height == 3);
  REQUIRE(dst.material_id.size() == src.material_id.size());
  for (size_t i = 0; i < src.material_id.size(); ++i) {
    CHECK(dst.material_id[i] == src.material_id[i]);
  }
  CHECK(static_cast<int>(dst.origpal.entries[0].r) == 0);
  CHECK(static_cast<int>(dst.origpal.entries[255].r) == 255);
}

TEST_CASE("cereal_types: Level round-trip preserves display layer", "[cereal_types][stage3]") {
  Common common;
  Level src(common);
  src.width = 4;
  src.height = 3;
  src.material_id.assign(12, 0);

  // Authored display layer: pixel 0 and pixel 5.
  src.display_data.assign(12, 0);
  src.display_valid.assign(12, 0);
  src.display_data[0] = 0xFF102030U;
  src.display_valid[0] = 1;
  src.display_data[5] = 0xFF405060U;
  src.display_valid[5] = 1;

  Level dst(common);
  std::stringstream ss;
  {
    cereal::PortableBinaryOutputArchive ar(ss);
    ar(cereal::make_nvp("lvl", src));
  }
  {
    cereal::PortableBinaryInputArchive ar(ss);
    ar(cereal::make_nvp("lvl", dst));
  }
  REQUIRE(dst.display_data.size() == 12);
  REQUIRE(dst.display_valid.size() == 12);
  CHECK(dst.display_data[0] == 0xFF102030U);
  CHECK(dst.display_valid[0] == 1);
  CHECK(dst.display_data[5] == 0xFF405060U);
  CHECK(dst.display_valid[5] == 1);
  CHECK(dst.display_valid[1] == 0);

  // Classic level (no display layer): round-trip produces empty vectors.
  Level src2(common);
  src2.width = 2;
  src2.height = 2;
  src2.material_id.assign(4, 0);
  Level dst2(common);
  std::stringstream ss2;
  {
    cereal::PortableBinaryOutputArchive ar(ss2);
    ar(cereal::make_nvp("lvl", src2));
  }
  {
    cereal::PortableBinaryInputArchive ar(ss2);
    ar(cereal::make_nvp("lvl", dst2));
  }
  CHECK(dst2.display_data.empty());
  CHECK(dst2.display_valid.empty());
}

TEST_CASE("cereal_types: Rand round-trip preserves stream", "[cereal_types]") {
  Rand src(0xC0FFEEU);
  for (int i = 0; i < 10; ++i) {
    src();
  }
  Rand bin = RoundtripBinary(src);
  CHECK(bin == src);
  CHECK(bin() == src());
}

TEST_CASE("cereal_types: Level round-trip preserves anim layer (stage4)",
          "[cereal_types][stage4]") {
  Common common;
  Level src(common);
  src.width = 3;
  src.height = 2;
  src.material_id.assign(6, 0);
  src.display_data.assign(6, 0U);
  src.display_valid.assign(6, 0);

  Level::ArgbRamp ramp;
  ramp.shift = 1;
  ramp.colors = {0xFF204060U, 0xFF408020U};
  src.argb_ramps.push_back(ramp);
  src.display_anim.assign(6, 0);
  src.display_anim[2] = 1;

  Level dst(common);
  std::stringstream ss;
  {
    cereal::PortableBinaryOutputArchive ar(ss);
    ar(cereal::make_nvp("lvl", src));
  }
  {
    cereal::PortableBinaryInputArchive ar(ss);
    ar(cereal::make_nvp("lvl", dst));
  }
  REQUIRE(dst.argb_ramps.size() == 1);
  CHECK(dst.argb_ramps[0].shift == 1);
  REQUIRE(dst.argb_ramps[0].colors.size() == 2);
  CHECK(dst.argb_ramps[0].colors[0] == 0xFF204060U);
  CHECK(dst.argb_ramps[0].colors[1] == 0xFF408020U);
  REQUIRE(dst.display_anim.size() == 6);
  CHECK(dst.display_anim[2] == 1);
  CHECK(dst.display_anim[0] == 0);
}

TEST_CASE("cereal_types: Level v8 stream loads with empty anim layer (stage4)",
          "[cereal_types][stage4]") {
  Common common;
  Level src(common);
  src.width = 2;
  src.height = 2;
  src.material_id.assign(4, 0);
  src.display_data.assign(4, 0xFF112233U);
  src.display_valid.assign(4, 1);

  // Write with version 8 (no anim data in stream).
  g_cereal_replay_version = 8;
  std::stringstream ss;
  {
    cereal::PortableBinaryOutputArchive ar(ss);
    ar(cereal::make_nvp("lvl", src));
  }

  Level dst(common);
  {
    cereal::PortableBinaryInputArchive ar(ss);
    ar(cereal::make_nvp("lvl", dst));
  }
  g_cereal_replay_version = kMyReplayVersion;

  CHECK(dst.display_data[0] == 0xFF112233U);
  CHECK(dst.argb_ramps.empty());
  CHECK(dst.display_anim.empty());
}

TEST_CASE("cereal_types: WormWeapon round-trip excludes type pointer", "[cereal_types]") {
  WormWeapon src;
  src.ammo = 17;
  src.delay_left = 23;
  src.loading_left = 41;

  // WormWeapon's default ctor leaves `type` uninitialized; this test
  // only checks the fields cereal serialize() touches.
  WormWeapon const kBin = RoundtripBinary(src);
  CHECK(kBin.ammo == 17);
  CHECK(kBin.delay_left == 23);
  CHECK(kBin.loading_left == 41);

  WormWeapon const kToml = RoundtripToml(src);
  CHECK(kToml.ammo == 17);
  CHECK(kToml.delay_left == 23);
  CHECK(kToml.loading_left == 41);
}
