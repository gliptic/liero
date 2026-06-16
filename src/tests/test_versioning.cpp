// Settings serialization regression tests: binary and TOML round-trips,
// plus backward-compatibility with old config files.
//
// The v1 binary fixture backward-compat test was removed when the
// serialization format was changed to always write bonusTimeout before
// wormSettings.
//
// Tests:
//   1. v2 binary round-trip — all fields preserved.
//   2. v2 TOML round-trip — all fields preserved (generic serialize path).
//   3. v1 TOML fixture → generic reader — missing fields get defaults.
//   4. Settings toToml/fromToml round-trip — human-readable config format.

#include <catch2/catch_test_macros.hpp>

#include <cereal/archives/portable_binary.hpp>
#include <cereal/cereal.hpp>

#include "game/serialization/cereal_types.hpp"
#include "game/serialization/toml_archive.hpp"

#include <sstream>

// ---------------------------------------------------------------------------
// v1 fixtures — old format TOML (ptr_wrapper style, tests generic serialize)
// ---------------------------------------------------------------------------

// v1 TOML fixture
static const char kSettingsV1Toml[] = R"toml([s]
aiFrames = 140
aiMutations = 2
aiParallels = 3
aiTraces = false
allowViewingSpawnPoint = false
blood = 100
bloodParticleMax = 700
cereal_class_version = 1
flagsToWin = 20
fullscreen = false
gameMode = 0
levelFile = ''
lives = 3
loadChange = true
loadPowerlevelPalette = true
loadingTime = 100
map = true
maxBonuses = 7
namesOnBonuses = false
randomLevel = true
recordReplays = true
regenerateLevel = false
screenSync = true
selectBotWeapons = 1
shadow = true
singleScreenReplay = false
spectatorWindow = false
tc = 'openliero'
timeToLose = 600
weap0 = 2
weap1 = 0
weap10 = 0
weap11 = 0
weap12 = 0
weap13 = 0
weap14 = 0
weap15 = 0
weap16 = 0
weap17 = 0
weap18 = 0
weap19 = 0
weap2 = 0
weap20 = 0
weap21 = 0
weap22 = 0
weap23 = 0
weap24 = 0
weap25 = 0
weap26 = 0
weap27 = 0
weap28 = 0
weap29 = 0
weap3 = 0
weap30 = 0
weap31 = 0
weap32 = 0
weap33 = 0
weap34 = 0
weap35 = 0
weap36 = 0
weap37 = 0
weap38 = 0
weap39 = 1
weap4 = 0
weap5 = 0
weap6 = 0
weap7 = 0
weap8 = 0
weap9 = 0
zoneTimeout = 30

    [s.worm0.ptr_wrapper]
    id = 2147483649

        [s.worm0.ptr_wrapper.data]
        color = 0
        control0 = 0
        control1 = 0
        control2 = 0
        control3 = 0
        control4 = 0
        control5 = 0
        control6 = 0
        controlEx0 = 0
        controlEx1 = 0
        controlEx2 = 0
        controlEx3 = 0
        controlEx4 = 0
        controlEx5 = 0
        controlEx6 = 0
        controlEx7 = 0
        controller = 0
        gamepadName = ''
        gamepadSerial = ''
        gpControl0 = 11
        gpControl1 = 12
        gpControl2 = 13
        gpControl3 = 14
        gpControl4 = 110
        gpControl5 = 10
        gpControl6 = 0
        gpControl7 = 9
        health = 100
        inputDevice = 0
        name = 'w0'
        randomName = true
        rgb0 = 26
        rgb1 = 26
        rgb2 = 62
        weapon0 = 1
        weapon1 = 1
        weapon2 = 1
        weapon3 = 1
        weapon4 = 1

    [s.worm1.ptr_wrapper]
    id = 2147483650

        [s.worm1.ptr_wrapper.data]
        color = 0
        control0 = 0
        control1 = 0
        control2 = 0
        control3 = 0
        control4 = 0
        control5 = 0
        control6 = 0
        controlEx0 = 0
        controlEx1 = 0
        controlEx2 = 0
        controlEx3 = 0
        controlEx4 = 0
        controlEx5 = 0
        controlEx6 = 0
        controlEx7 = 0
        controller = 0
        gamepadName = ''
        gamepadSerial = ''
        gpControl0 = 11
        gpControl1 = 12
        gpControl2 = 13
        gpControl3 = 14
        gpControl4 = 110
        gpControl5 = 10
        gpControl6 = 0
        gpControl7 = 9
        health = 101
        inputDevice = 0
        name = 'w1'
        randomName = true
        rgb0 = 26
        rgb1 = 26
        rgb2 = 62
        weapon0 = 1
        weapon1 = 1
        weapon2 = 1
        weapon3 = 1
        weapon4 = 1

    [s.worm2.ptr_wrapper]
    id = 2147483651

        [s.worm2.ptr_wrapper.data]
        color = 0
        control0 = 0
        control1 = 0
        control2 = 0
        control3 = 0
        control4 = 0
        control5 = 0
        control6 = 0
        controlEx0 = 0
        controlEx1 = 0
        controlEx2 = 0
        controlEx3 = 0
        controlEx4 = 0
        controlEx5 = 0
        controlEx6 = 0
        controlEx7 = 0
        controller = 0
        gamepadName = ''
        gamepadSerial = ''
        gpControl0 = 11
        gpControl1 = 12
        gpControl2 = 13
        gpControl3 = 14
        gpControl4 = 110
        gpControl5 = 10
        gpControl6 = 0
        gpControl7 = 9
        health = 102
        inputDevice = 0
        name = 'w2'
        randomName = true
        rgb0 = 26
        rgb1 = 26
        rgb2 = 62
        weapon0 = 1
        weapon1 = 1
        weapon2 = 1
        weapon3 = 1
        weapon4 = 1
)toml";

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static Settings MakeKnownV2() {
  Settings s;
  s.max_bonuses = 7;
  s.lives = 3;
  s.shadow = true;
  s.tc = "openliero";
  s.weap_table[0] = 2;
  s.weap_table[39] = 1;
  s.bonus_timeout = 42;
  for (int i = 0; i < Settings::kNumWormSettings; ++i) {
    s.worm_settings[i] = std::make_shared<WormSettings>();
    s.worm_settings[i]->health = 100 + i;
    s.worm_settings[i]->name = "w" + std::to_string(i);
  }
  return s;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_CASE("versioning: Settings v2 binary round-trip preserves bonusTimeout", "[versioning]") {
  Settings src = MakeKnownV2();
  std::stringstream ss;
  {
    cereal::PortableBinaryOutputArchive ar(ss);
    ar(cereal::make_nvp("s", src));
  }
  Settings dst;
  {
    cereal::PortableBinaryInputArchive ar(ss);
    ar(cereal::make_nvp("s", dst));
  }
  CHECK(dst.max_bonuses == 7);
  CHECK(dst.lives == 3);
  CHECK(dst.bonus_timeout == 42);
  CHECK(dst.worm_settings[0]->health == 100);
  CHECK(dst.worm_settings[2]->name == "w2");
}

TEST_CASE("versioning: Settings v2 TOML round-trip preserves bonusTimeout", "[versioning]") {
  Settings src = MakeKnownV2();
  std::stringstream ss;
  {
    cereal::TomlOutputArchive ar(ss);
    ar(cereal::make_nvp("s", src));
  }
  // bonusTimeout = 42 must appear in the serialized text.
  CHECK(ss.str().contains("bonusTimeout"));

  Settings dst;
  {
    cereal::TomlInputArchive ar(ss);
    ar(cereal::make_nvp("s", dst));
  }
  CHECK(dst.max_bonuses == 7);
  CHECK(dst.lives == 3);
  CHECK(dst.bonus_timeout == 42);
  CHECK(dst.worm_settings[2]->health == 102);
}

TEST_CASE("versioning: Settings v1 TOML fixture reads as v2 with default bonusTimeout",
          "[versioning]") {
  std::stringstream ss(kSettingsV1Toml);
  Settings dst;
  {
    cereal::TomlInputArchive ar(ss);
    ar(cereal::make_nvp("s", dst));
  }
  // Fields present in v1.
  CHECK(dst.max_bonuses == 7);
  CHECK(dst.lives == 3);
  CHECK(dst.shadow == true);
  CHECK(dst.tc == "openliero");
  CHECK(dst.worm_settings[0]->health == 100);
  CHECK(dst.worm_settings[1]->name == "w1");
  // bonusTimeout absent in v1 TOML — must be the struct default (0).
  CHECK(dst.bonus_timeout == 0);
}

TEST_CASE("versioning: Settings toToml/fromToml produces human-readable config", "[versioning]") {
  Settings const kSrc = MakeKnownV2();
  kSrc.worm_settings[0]->name = "Player1";
  kSrc.worm_settings[1]->name = "Player2";
  kSrc.worm_settings[2]->name = "NetPlayer";

  std::string const kToml = kSrc.ToToml();

  // Human-readable: uses [settings], [player1], [player2], [network_player]
  CHECK(kToml.contains("[settings]"));
  CHECK(kToml.contains("[player1]"));
  CHECK(kToml.contains("[player2]"));
  CHECK(kToml.contains("[network_player]"));
  // Version field present for future-proofing
  CHECK(kToml.contains("version = 6"));
  // No ptr_wrapper noise
  CHECK(!kToml.contains("ptr_wrapper"));
  CHECK(!kToml.contains("[s]"));
  // Arrays used for weapons, controls, etc.
  CHECK(kToml.contains("weapons = ["));
  CHECK(kToml.contains("controls = ["));
  CHECK(kToml.contains("weapTable = ["));

  // Round-trip
  Settings dst;
  dst.FromToml(kToml);
  CHECK(dst.max_bonuses == 7);
  CHECK(dst.lives == 3);
  CHECK(dst.bonus_timeout == 42);
  CHECK(dst.worm_settings[0]->name == "Player1");
  CHECK(dst.worm_settings[1]->name == "Player2");
  CHECK(dst.worm_settings[2]->name == "NetPlayer");
  CHECK(dst.worm_settings[0]->health == 100);
  CHECK(dst.worm_settings[2]->health == 102);
}

TEST_CASE("versioning: worm rgb TOML round-trips 0..255 with rgbDepth marker", "[versioning]") {
  WormSettings src;
  src.rgb[0] = 255;
  src.rgb[1] = 128;
  src.rgb[2] = 33;  // not a multiple of 4: must survive exactly

  std::string const kToml = src.ToToml();
  CHECK(kToml.contains("rgbDepth = 8"));

  WormSettings dst;
  dst.FromToml(kToml);
  CHECK(dst.rgb[0] == 255);
  CHECK(dst.rgb[1] == 128);
  CHECK(dst.rgb[2] == 33);
}

TEST_CASE("versioning: worm TOML without rgbDepth expands 6-bit rgb", "[versioning]") {
  // Old profiles / configs stored channels in 0..63 and had no marker.
  WormSettings dst;
  dst.FromToml("rgb = [26, 26, 63]\n");
  CHECK(dst.rgb[0] == 104);
  CHECK(dst.rgb[1] == 104);
  CHECK(dst.rgb[2] == 252);
}

TEST_CASE("versioning: settings TOML without rgbDepth expands worm rgb", "[versioning]") {
  Settings const kSrc;
  std::string toml = kSrc.ToToml();
  // Simulate an old config: strip the markers and write 6-bit channels.
  std::string::size_type pos;
  while ((pos = toml.find("rgbDepth = 8")) != std::string::npos) {
    toml.erase(pos, std::string("rgbDepth = 8").length());
  }
  // Sections render alphabetically; anchor on [player1] to hit its rgb.
  pos = toml.find("[player1]");
  REQUIRE(pos != std::string::npos);
  pos = toml.find("rgb = [", pos);
  REQUIRE(pos != std::string::npos);
  toml.replace(pos, toml.find(']', pos) - pos + 1, "rgb = [15, 43, 15]");

  Settings dst;
  dst.FromToml(toml);
  CHECK(dst.worm_settings[0]->rgb[0] == 60);
  CHECK(dst.worm_settings[0]->rgb[1] == 172);
  CHECK(dst.worm_settings[0]->rgb[2] == 60);
}

TEST_CASE("versioning: modernColors round-trips and defaults to classic", "[versioning]") {
  Settings src;
  src.modern_colors = true;
  std::string const kToml = src.ToToml();
  CHECK(kToml.contains("modernColors = true"));

  Settings dst;
  dst.FromToml(kToml);
  CHECK(dst.modern_colors == true);

  // Configs predating the field stay classic (missing key keeps the
  // struct default).
  Settings legacy;
  std::string toml = kToml;
  auto const kPos = toml.find("modernColors = true");
  REQUIRE(kPos != std::string::npos);
  toml.replace(kPos, std::string("modernColors = true").length(), "");
  legacy.FromToml(toml);
  CHECK(legacy.modern_colors == false);
}

TEST_CASE("versioning: maxSpectatorRenderHeight round-trips and defaults to 1080", "[versioning]") {
  Settings src;
  src.max_spectator_render_height = 1440;
  std::string const kToml = src.ToToml();
  CHECK(kToml.contains("maxSpectatorRenderHeight = 1440"));

  Settings dst;
  dst.FromToml(kToml);
  CHECK(dst.max_spectator_render_height == 1440);

  // Configs predating the v6 field keep the struct default (1080).
  Settings legacy;
  std::string toml = kToml;
  auto const kPos = toml.find("maxSpectatorRenderHeight = 1440");
  REQUIRE(kPos != std::string::npos);
  toml.replace(kPos, std::string("maxSpectatorRenderHeight = 1440").length(), "");
  legacy.FromToml(toml);
  CHECK(legacy.max_spectator_render_height == 1080);
}

TEST_CASE("versioning: out-of-range worm rgb in TOML is clamped on load", "[versioning]") {
  // A picker bug briefly stored 256; loads must clamp into 0..255.
  WormSettings dst;
  dst.FromToml("rgbDepth = 8\nrgb = [256, -4, 300]\n");
  CHECK(dst.rgb[0] == 255);
  CHECK(dst.rgb[1] == 0);
  CHECK(dst.rgb[2] == 255);
}
