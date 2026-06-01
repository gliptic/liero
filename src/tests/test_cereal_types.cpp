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
T roundtripBinary(T const& src) {
  std::stringstream ss;
  {
    cereal::PortableBinaryOutputArchive ar(ss);
    ar(cereal::make_nvp("v", src));
  }
  T dst{};
  {
    cereal::PortableBinaryInputArchive ar(ss);
    ar(cereal::make_nvp("v", dst));
  }
  return dst;
}

template <typename T>
T roundtripToml(T const& src) {
  std::stringstream ss;
  {
    cereal::TomlOutputArchive ar(ss);
    ar(cereal::make_nvp("v", src));
  }
  T dst{};
  {
    cereal::TomlInputArchive ar(ss);
    ar(cereal::make_nvp("v", dst));
  }
  return dst;
}

}  // namespace

TEST_CASE("cereal_types: BasicVec<int,2> binary round-trip", "[cereal_types]") {
  IVec2 src{-12345, 6789};
  IVec2 dst = roundtripBinary(src);
  CHECK(dst.x == src.x);
  CHECK(dst.y == src.y);
}

TEST_CASE("cereal_types: BasicVec<int,2> toml round-trip", "[cereal_types]") {
  IVec2 src{-12345, 6789};
  IVec2 dst = roundtripToml(src);
  CHECK(dst.x == src.x);
  CHECK(dst.y == src.y);
}

TEST_CASE("cereal_types: BasicVec<float,2> binary round-trip", "[cereal_types]") {
  FVec2 src{1.5f, -2.25f};
  FVec2 dst = roundtripBinary(src);
  CHECK(dst.x == src.x);
  CHECK(dst.y == src.y);
}

TEST_CASE("cereal_types: BasicRect<int> binary round-trip", "[cereal_types]") {
  Rect src{1, 2, 100, 200};
  Rect dst = roundtripBinary(src);
  CHECK(dst.x1 == 1);
  CHECK(dst.y1 == 2);
  CHECK(dst.x2 == 100);
  CHECK(dst.y2 == 200);
}

TEST_CASE("cereal_types: BasicRect<int> toml round-trip", "[cereal_types]") {
  Rect src{1, 2, 100, 200};
  Rect dst = roundtripToml(src);
  CHECK(dst.x1 == 1);
  CHECK(dst.y1 == 2);
  CHECK(dst.x2 == 100);
  CHECK(dst.y2 == 200);
}

TEST_CASE("cereal_types: ControlState round-trip", "[cereal_types]") {
  Worm::ControlState src;
  src.istate = 0x5a;
  Worm::ControlState bin = roundtripBinary(src);
  Worm::ControlState toml = roundtripToml(src);
  CHECK(bin.istate == src.istate);
  CHECK(toml.istate == src.istate);
}

TEST_CASE("cereal_types: Ninjarope round-trip excludes anchor", "[cereal_types]") {
  Ninjarope src;
  src.out = true;
  src.attached = true;
  src.pos = fixedvec{1234, -5678};
  src.vel = fixedvec{-1, 2};
  src.length = 700;
  src.curLen = 350;
  // anchor is intentionally not serialized; verify dst.anchor stays default.
  src.anchor = reinterpret_cast<Worm*>(0xdeadbeef);

  Ninjarope bin = roundtripBinary(src);
  CHECK(bin.out == true);
  CHECK(bin.attached == true);
  CHECK(bin.pos.x == 1234);
  CHECK(bin.pos.y == -5678);
  CHECK(bin.vel.x == -1);
  CHECK(bin.vel.y == 2);
  CHECK(bin.length == 700);
  CHECK(bin.curLen == 350);
  CHECK(bin.anchor == nullptr);
}

TEST_CASE("cereal_types: Viewport round-trip", "[cereal_types]") {
  Viewport src;
  src.x = 100;
  src.y = -50;
  src.shake = 7;
  src.maxX = 320;
  src.maxY = 200;
  src.centerX = 160;
  src.centerY = 100;
  src.wormIdx = 1;
  src.bannerY = -8;
  src.rect = Rect{10, 20, 30, 40};

  Viewport bin = roundtripBinary(src);
  CHECK(bin.x == 100);
  CHECK(bin.y == -50);
  CHECK(bin.shake == 7);
  CHECK(bin.wormIdx == 1);
  CHECK(bin.rect.x1 == 10);
  CHECK(bin.rect.y2 == 40);
}

TEST_CASE("cereal_types: Worm round-trip excludes context fields", "[cereal_types]") {
  Worm src;
  src.pos = fixedvec{1 << 16, 2 << 16};
  src.vel = fixedvec{-3, 4};
  src.logicRespawn = IVec2{50, 60};
  src.health = 75;
  src.lives = 3;
  src.kills = 11;
  src.aimingAngle = 90 << 16;
  src.lastKilledByIdx = 2;
  src.currentWeapon = 3;
  src.direction = 1;
  src.controlStates.istate = 0x42;
  src.weapons[0].ammo = 99;
  src.weapons[2].delayLeft = 17;
  src.reacts[1] = 5;
  src.ninjarope.out = true;
  src.ninjarope.length = 500;

  Worm bin = roundtripBinary(src);
  CHECK(bin.pos.x == src.pos.x);
  CHECK(bin.pos.y == src.pos.y);
  CHECK(bin.logicRespawn.x == 50);
  CHECK(bin.health == 75);
  CHECK(bin.lives == 3);
  CHECK(bin.aimingAngle == 90 << 16);
  CHECK(bin.lastKilledByIdx == 2);
  CHECK(bin.currentWeapon == 3);
  CHECK(bin.direction == 1);
  CHECK(bin.controlStates.istate == 0x42);
  CHECK(bin.weapons[0].ammo == 99);
  CHECK(bin.weapons[2].delayLeft == 17);
  CHECK(bin.reacts[1] == 5);
  CHECK(bin.ninjarope.out == true);
  CHECK(bin.ninjarope.length == 500);

  Worm toml = roundtripToml(src);
  CHECK(toml.pos.x == src.pos.x);
  CHECK(toml.health == 75);
  CHECK(toml.weapons[0].ammo == 99);
  CHECK(toml.ninjarope.length == 500);
}

TEST_CASE("cereal_types: WormSettings round-trip", "[cereal_types]") {
  WormSettings src;
  src.health = 150;
  src.controller = 1;
  src.name = "Hero";
  src.randomName = false;
  src.color = 3;
  src.rgb[0] = 10;
  src.rgb[1] = 20;
  src.rgb[2] = 30;
  src.weapons[0] = 5;
  src.weapons[4] = 7;
  src.controls[WormSettings::Up] = 100u;

  WormSettings bin = roundtripBinary(src);
  CHECK(bin.health == 150);
  CHECK(bin.controller == 1);
  CHECK(bin.name == "Hero");
  CHECK(bin.randomName == false);
  CHECK(bin.color == 3);
  CHECK(bin.rgb[0] == 10);
  CHECK(bin.rgb[2] == 30);
  CHECK(bin.weapons[0] == 5);
  CHECK(bin.weapons[4] == 7);
  CHECK(bin.controls[WormSettings::Up] == 100u);

  WormSettings toml = roundtripToml(src);
  CHECK(toml.name == "Hero");
  CHECK(toml.weapons[4] == 7);
}

TEST_CASE("cereal_types: Settings round-trip with shared worm settings", "[cereal_types]") {
  Settings src;
  src.maxBonuses = 12;
  src.lives = 5;
  src.shadow = true;
  src.tc = "openliero";
  src.weapTable[0] = 2;
  src.weapTable[39] = 1;
  for (int i = 0; i < Settings::NumWormSettings; ++i) {
    src.wormSettings[i] = std::make_shared<WormSettings>();
    src.wormSettings[i]->health = 100 + i;
    src.wormSettings[i]->name = "worm" + std::to_string(i);
  }

  Settings bin = roundtripBinary(src);
  CHECK(bin.maxBonuses == 12);
  CHECK(bin.lives == 5);
  CHECK(bin.shadow == true);
  CHECK(bin.tc == "openliero");
  CHECK(bin.weapTable[0] == 2);
  CHECK(bin.weapTable[39] == 1);
  for (int i = 0; i < Settings::NumWormSettings; ++i) {
    REQUIRE(bin.wormSettings[i] != nullptr);
    CHECK(bin.wormSettings[i]->health == 100 + i);
    CHECK(bin.wormSettings[i]->name == "worm" + std::to_string(i));
  }
}

TEST_CASE("cereal_types: Color round-trip", "[cereal_types]") {
  Color src{10, 20, 30, 0};
  Color bin = roundtripBinary(src);
  CHECK(bin.r == 10);
  CHECK(bin.g == 20);
  CHECK(bin.b == 30);
}

TEST_CASE("cereal_types: Palette round-trip", "[cereal_types]") {
  Palette src;
  for (int i = 0; i < 256; ++i) {
    src.entries[i].r = static_cast<uint8_t>(i);
    src.entries[i].g = static_cast<uint8_t>(255 - i);
    src.entries[i].b = static_cast<uint8_t>(i ^ 0x55);
  }
  Palette bin = roundtripBinary(src);
  for (int i = 0; i < 256; ++i) {
    CHECK(static_cast<int>(bin.entries[i].r) == i);
    CHECK(static_cast<int>(bin.entries[i].g) == 255 - i);
    CHECK(static_cast<int>(bin.entries[i].b) == (i ^ 0x55));
  }
}

TEST_CASE("cereal_types: Level round-trip preserves data and palette", "[cereal_types]") {
  Common common;
  Level src(common);
  src.width = 4;
  src.height = 3;
  src.data.assign(src.width * src.height, 0);
  for (int i = 0; i < src.width * src.height; ++i)
    src.data[i] = static_cast<unsigned char>(i * 7 + 1);
  for (int i = 0; i < 256; ++i) src.origpal.entries[i].r = static_cast<uint8_t>(i);

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
  REQUIRE(dst.data.size() == src.data.size());
  for (size_t i = 0; i < src.data.size(); ++i) CHECK(dst.data[i] == src.data[i]);
  CHECK(static_cast<int>(dst.origpal.entries[0].r) == 0);
  CHECK(static_cast<int>(dst.origpal.entries[255].r) == 255);
}

TEST_CASE("cereal_types: Rand round-trip preserves stream", "[cereal_types]") {
  Rand src(0xC0FFEEu);
  for (int i = 0; i < 10; ++i) src();
  Rand bin = roundtripBinary(src);
  CHECK(bin == src);
  CHECK(bin() == src());
}

TEST_CASE("cereal_types: WormWeapon round-trip excludes type pointer", "[cereal_types]") {
  WormWeapon src;
  src.ammo = 17;
  src.delayLeft = 23;
  src.loadingLeft = 41;

  // WormWeapon's default ctor leaves `type` uninitialized; this test
  // only checks the fields cereal serialize() touches.
  WormWeapon bin = roundtripBinary(src);
  CHECK(bin.ammo == 17);
  CHECK(bin.delayLeft == 23);
  CHECK(bin.loadingLeft == 41);

  WormWeapon toml = roundtripToml(src);
  CHECK(toml.ammo == 17);
  CHECK(toml.delayLeft == 23);
  CHECK(toml.loadingLeft == 41);
}
