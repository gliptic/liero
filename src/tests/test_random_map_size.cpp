#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>

#include "common.hpp"
#include "filesystem.hpp"
#include "level.hpp"
#include "rand.hpp"
#include "settings.hpp"

// ---------------------------------------------------------------------------
// Settings field defaults
// ---------------------------------------------------------------------------

TEST_CASE("Settings random_map_width defaults to 504", "[random-map-size]") {
  Settings const kS;
  CHECK(kS.random_map_width == 504);
}

TEST_CASE("Settings random_map_height defaults to 350", "[random-map-size]") {
  Settings const kS;
  CHECK(kS.random_map_height == 350);
}

TEST_CASE("Settings config version is 5", "[random-map-size]") {
  CHECK(Settings::kConfigVersion == 5);
}

// ---------------------------------------------------------------------------
// TOML serialization
// ---------------------------------------------------------------------------

TEST_CASE("Settings random_map dimensions round-trip through TOML", "[random-map-size]") {
  Settings original;
  original.random_map_width = 256;
  original.random_map_height = 192;

  std::string const kToml = original.ToToml();

  Settings loaded;
  loaded.FromToml(kToml);

  CHECK(loaded.random_map_width == 256);
  CHECK(loaded.random_map_height == 192);
}

TEST_CASE("Settings TOML contains version = 5", "[random-map-size]") {
  Settings const kSettings;
  CHECK(kSettings.ToToml().contains("version = 5"));
}

// Configs written before v5 lack randomMapWidth/randomMapHeight.
// Loading them should silently fall back to the 504x350 struct defaults.
TEST_CASE("Config without random_map fields falls back to 504x350 defaults", "[random-map-size]") {
  Settings original;
  original.random_map_width = 128;
  original.random_map_height = 96;

  std::string toml = original.ToToml();

  // Strip the new fields to simulate a pre-v5 config
  auto remove_line = [&](std::string const& prefix) {
    auto pos = toml.find(prefix);
    if (pos != std::string::npos) {
      auto end = toml.find('\n', pos);
      if (end != std::string::npos) {
        ++end;
      }
      toml.erase(pos, end - pos);
    }
  };
  remove_line("randomMapWidth");
  remove_line("randomMapHeight");

  Settings loaded;
  loaded.FromToml(toml);

  CHECK(loaded.random_map_width == 504);
  CHECK(loaded.random_map_height == 350);
}

// ---------------------------------------------------------------------------
// Level generation uses the configured dimensions (requires TC assets)
// ---------------------------------------------------------------------------

TEST_CASE("GenerateFromSettings uses random_map_width/height", "[random-map-size]") {
  auto common = std::make_shared<Common>();
  FsNode const kTcRoot(FsNode("data") / "TC" / "openliero");
  common->load(kTcRoot);

  Settings settings;
  settings.random_level = true;
  // Use width != 504 and height != 350 to prove the setting is being used.
  // Large enough that GenerateRandom's rock-placement loops have room to
  // find clear areas (a 128x96 map overflows with rocks and loops forever).
  settings.random_map_width = 600;
  settings.random_map_height = 350;

  Rand rand;
  rand.Seed(42);

  Level level(*common);
  level.GenerateFromSettings(*common, settings, rand);

  CHECK(level.width == 600);
  CHECK(level.height == 350);
}

TEST_CASE("GenerateFromSettings default dimensions produce 504x350 level", "[random-map-size]") {
  auto common = std::make_shared<Common>();
  FsNode const kTcRoot(FsNode("data") / "TC" / "openliero");
  common->load(kTcRoot);

  Settings settings;
  settings.random_level = true;
  // random_map_width/height at struct defaults (504, 350)

  Rand rand;
  rand.Seed(1);

  Level level(*common);
  level.GenerateFromSettings(*common, settings, rand);

  CHECK(level.width == 504);
  CHECK(level.height == 350);
}
