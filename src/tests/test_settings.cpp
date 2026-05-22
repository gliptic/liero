#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>

#include "filesystem.hpp"
#include "rand.hpp"
#include "settings.hpp"

TEST_CASE("Settings round-trip via TOML") {
  Rand rand;
  rand.seed(42);

  // Create settings with non-default values
  Settings original;
  original.maxBonuses = 7;
  original.blood = 200;
  original.timeToLose = 300;
  original.flagsToWin = 10;
  original.gameMode = 2;
  original.shadow = false;
  original.loadChange = false;
  original.namesOnBonuses = true;
  original.regenerateLevel = true;
  original.lives = 25;
  original.loadingTime = 50;
  original.randomLevel = false;
  original.levelFile = "testlevel.lev";
  original.map = false;
  original.screenSync = false;

  // Extension fields
  original.recordReplays = false;
  original.loadPowerlevelPalette = false;
  original.bloodParticleMax = 500;
  original.aiFrames = 200;
  original.aiMutations = 5;
  original.aiTraces = true;
  original.aiParallels = 8;
  original.fullscreen = true;
  original.zoneTimeout = 60;
  original.selectBotWeapons = 2;
  original.allowViewingSpawnPoint = true;
  original.singleScreenReplay = true;
  original.spectatorWindow = true;
  original.tc = "customtc";

  // Weapon table
  for (int i = 0; i < 40; ++i)
    original.weapTable[i] = i % 3;

  // Worm settings (player 0)
  original.wormSettings[0]->controller = 1;
  original.wormSettings[0]->health = 150;
  original.wormSettings[0]->name = "TestWorm1";
  original.wormSettings[0]->randomName = false;
  original.wormSettings[0]->rgb[0] = 10;
  original.wormSettings[0]->rgb[1] = 20;
  original.wormSettings[0]->rgb[2] = 30;
  original.wormSettings[0]->controlsEx[0] = 100;
  original.wormSettings[0]->controlsEx[1] = 101;
  original.wormSettings[0]->weapons[0] = 5;
  original.wormSettings[0]->weapons[1] = 10;
  original.wormSettings[0]->inputDevice = 1;
  original.wormSettings[0]->gamepadName = "Xbox Controller";

  // Worm settings (player 1)
  original.wormSettings[1]->controller = 2;
  original.wormSettings[1]->health = 200;
  original.wormSettings[1]->name = "TestWorm2";
  original.wormSettings[1]->randomName = false;
  original.wormSettings[1]->rgb[0] = 40;
  original.wormSettings[1]->rgb[1] = 50;
  original.wormSettings[1]->rgb[2] = 60;
  original.wormSettings[1]->controlsEx[2] = 200;

  // Network player (player 2)
  original.wormSettings[2]->controller = 1;
  original.wormSettings[2]->health = 175;
  original.wormSettings[2]->name = "NetPlayer";
  original.wormSettings[2]->randomName = false;
  original.wormSettings[2]->rgb[0] = 5;
  original.wormSettings[2]->rgb[1] = 15;
  original.wormSettings[2]->rgb[2] = 25;
  original.wormSettings[2]->controlsEx[0] = 50;
  original.wormSettings[2]->controlsEx[3] = 77;
  original.wormSettings[2]->weapons[0] = 3;
  original.wormSettings[2]->weapons[2] = 7;
  original.wormSettings[2]->inputDevice = 2;
  original.wormSettings[2]->gamepadName = "PS5 Controller";
  original.wormSettings[2]->gamepadSerial = "ABC123";
  original.wormSettings[2]->gamepadControls[0] = 10;

  // Save to temp file
  auto tmpPath =
      std::filesystem::temp_directory_path() / "openliero_test_settings.cfg";
  FsNode node(tmpPath.string());

  original.save(node, rand);

  // Load into a fresh Settings object
  Settings loaded;
  REQUIRE(loaded.load(node, rand));

  // Verify base settings
  CHECK(loaded.maxBonuses == original.maxBonuses);
  CHECK(loaded.blood == original.blood);
  CHECK(loaded.timeToLose == original.timeToLose);
  CHECK(loaded.flagsToWin == original.flagsToWin);
  CHECK(loaded.gameMode == original.gameMode);
  CHECK(loaded.shadow == original.shadow);
  CHECK(loaded.loadChange == original.loadChange);
  CHECK(loaded.namesOnBonuses == original.namesOnBonuses);
  CHECK(loaded.regenerateLevel == original.regenerateLevel);
  CHECK(loaded.lives == original.lives);
  CHECK(loaded.loadingTime == original.loadingTime);
  CHECK(loaded.randomLevel == original.randomLevel);
  CHECK(loaded.levelFile == original.levelFile);
  CHECK(loaded.map == original.map);
  CHECK(loaded.screenSync == original.screenSync);

  // Extension fields
  CHECK(loaded.recordReplays == original.recordReplays);
  CHECK(loaded.loadPowerlevelPalette == original.loadPowerlevelPalette);
  CHECK(loaded.bloodParticleMax == original.bloodParticleMax);
  CHECK(loaded.aiFrames == original.aiFrames);
  CHECK(loaded.aiMutations == original.aiMutations);
  CHECK(loaded.aiTraces == original.aiTraces);
  CHECK(loaded.aiParallels == original.aiParallels);
  CHECK(loaded.fullscreen == original.fullscreen);
  CHECK(loaded.zoneTimeout == original.zoneTimeout);
  CHECK(loaded.selectBotWeapons == original.selectBotWeapons);
  CHECK(loaded.allowViewingSpawnPoint == original.allowViewingSpawnPoint);
  CHECK(loaded.singleScreenReplay == original.singleScreenReplay);
  CHECK(loaded.spectatorWindow == original.spectatorWindow);
  CHECK(loaded.tc == original.tc);

  // Weapon table
  for (int i = 0; i < 40; ++i)
    CHECK(loaded.weapTable[i] == original.weapTable[i]);

  // Player 0
  CHECK(loaded.wormSettings[0]->controller == original.wormSettings[0]->controller);
  CHECK(loaded.wormSettings[0]->health == original.wormSettings[0]->health);
  CHECK(loaded.wormSettings[0]->name == original.wormSettings[0]->name);
  CHECK(loaded.wormSettings[0]->rgb[0] == original.wormSettings[0]->rgb[0]);
  CHECK(loaded.wormSettings[0]->rgb[1] == original.wormSettings[0]->rgb[1]);
  CHECK(loaded.wormSettings[0]->rgb[2] == original.wormSettings[0]->rgb[2]);
  CHECK(loaded.wormSettings[0]->controlsEx[0] == original.wormSettings[0]->controlsEx[0]);
  CHECK(loaded.wormSettings[0]->controlsEx[1] == original.wormSettings[0]->controlsEx[1]);
  CHECK(loaded.wormSettings[0]->weapons[0] == original.wormSettings[0]->weapons[0]);
  CHECK(loaded.wormSettings[0]->weapons[1] == original.wormSettings[0]->weapons[1]);
  CHECK(loaded.wormSettings[0]->inputDevice == original.wormSettings[0]->inputDevice);
  CHECK(loaded.wormSettings[0]->gamepadName == original.wormSettings[0]->gamepadName);

  // Player 1
  CHECK(loaded.wormSettings[1]->controller == original.wormSettings[1]->controller);
  CHECK(loaded.wormSettings[1]->health == original.wormSettings[1]->health);
  CHECK(loaded.wormSettings[1]->name == original.wormSettings[1]->name);
  CHECK(loaded.wormSettings[1]->rgb[0] == original.wormSettings[1]->rgb[0]);
  CHECK(loaded.wormSettings[1]->controlsEx[2] == original.wormSettings[1]->controlsEx[2]);

  // Network player (the critical one that was broken)
  CHECK(loaded.wormSettings[2]->controller == original.wormSettings[2]->controller);
  CHECK(loaded.wormSettings[2]->health == original.wormSettings[2]->health);
  CHECK(loaded.wormSettings[2]->name == original.wormSettings[2]->name);
  CHECK(loaded.wormSettings[2]->rgb[0] == original.wormSettings[2]->rgb[0]);
  CHECK(loaded.wormSettings[2]->rgb[1] == original.wormSettings[2]->rgb[1]);
  CHECK(loaded.wormSettings[2]->rgb[2] == original.wormSettings[2]->rgb[2]);
  CHECK(loaded.wormSettings[2]->controlsEx[0] == original.wormSettings[2]->controlsEx[0]);
  CHECK(loaded.wormSettings[2]->controlsEx[3] == original.wormSettings[2]->controlsEx[3]);
  CHECK(loaded.wormSettings[2]->weapons[0] == original.wormSettings[2]->weapons[0]);
  CHECK(loaded.wormSettings[2]->weapons[2] == original.wormSettings[2]->weapons[2]);
  CHECK(loaded.wormSettings[2]->inputDevice == original.wormSettings[2]->inputDevice);
  CHECK(loaded.wormSettings[2]->gamepadName == original.wormSettings[2]->gamepadName);
  CHECK(loaded.wormSettings[2]->gamepadSerial == original.wormSettings[2]->gamepadSerial);
  CHECK(loaded.wormSettings[2]->gamepadControls[0] == original.wormSettings[2]->gamepadControls[0]);

  // Cleanup
  std::filesystem::remove(tmpPath);
}

TEST_CASE("Settings load handles missing file gracefully") {
  Rand rand;
  rand.seed(1);

  Settings settings;
  FsNode node("/tmp/nonexistent_openliero_settings.cfg");
  CHECK_FALSE(settings.load(node, rand));
}

TEST_CASE("Settings TOML with comments") {
  Rand rand;
  rand.seed(42);

  // Write a minimal config, then manually inject a comment
  Settings original;
  original.maxBonuses = 12;
  original.blood = 50;

  auto tmpPath =
      std::filesystem::temp_directory_path() / "openliero_test_comments.cfg";

  // Write with comment manually
  {
    std::ofstream f(tmpPath);
    f << "# This is a comment\n";
    f << "maxBonuses = 12\n";
    f << "blood = 50\n";
    f << "loadingTime = 100\n";
    f << "lives = 15\n";
    f << "timeToLose = 600\n";
    f << "flagsToWin = 20\n";
    f << "screenSync = true\n";
    f << "map = true\n";
    f << "randomLevel = true\n";
    f << "gameMode = 0\n";
    f << "namesOnBonuses = false\n";
    f << "regenerateLevel = false\n";
    f << "shadow = true\n";
    f << "loadChange = true\n";
    f << "levelFile = \"\"\n";
    f << "recordReplays = true\n";
    f << "loadPowerlevelPalette = true\n";
    f << "aiMutations = 2\n";
    f << "aiFrames = 140\n";
    f << "selectBotWeapons = 1\n";
    f << "zoneTimeout = 30\n";
    f << "aiTraces = false\n";
    f << "aiParallels = 3\n";
    f << "allowViewingSpawnPoint = false\n";
    f << "singleScreenReplay = false\n";
    f << "spectatorWindow = false\n";
    f << "fullscreen = false\n";
    f << "tc = \"openliero\"\n";
    f << "bloodParticleMax = 700\n";
    f << "weapTable = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]\n";
    f << "\n";
    f << "[[worms]]\n";
    f << "controller = 0\n";
    f << "color = [26, 26, 63]\n";
    f << "weapons = [1, 1, 1, 1, 1]\n";
    f << "health = 100\n";
    f << "name = \"\"\n";
    f << "controls = [0, 0, 0, 0, 0, 0, 0, 0]\n";
    f << "inputDevice = 0\n";
    f << "gamepadName = \"\"\n";
    f << "gamepadSerial = \"\"\n";
    f << "gamepadControls = [0, 0, 0, 0, 0, 0, 0, 0]\n";
    f << "\n";
    f << "[[worms]]\n";
    f << "controller = 0\n";
    f << "color = [15, 43, 15]\n";
    f << "weapons = [1, 1, 1, 1, 1]\n";
    f << "health = 100\n";
    f << "name = \"\"\n";
    f << "controls = [0, 0, 0, 0, 0, 0, 0, 0]\n";
    f << "inputDevice = 0\n";
    f << "gamepadName = \"\"\n";
    f << "gamepadSerial = \"\"\n";
    f << "gamepadControls = [0, 0, 0, 0, 0, 0, 0, 0]\n";
  }

  FsNode node(tmpPath.string());
  Settings loaded;
  REQUIRE(loaded.load(node, rand));
  std::filesystem::remove(tmpPath);
}

TEST_CASE("Settings hash only includes gameplay fields") {
  Settings s1;
  Settings s2;

  // Both should have the same hash initially
  auto hash1 = s1.updateHash();
  auto hash2 = s2.updateHash();
  CHECK(hash1 == hash2);

  // Changing an app-only field should NOT change the hash
  s2.fullscreen = !s2.fullscreen;
  s2.singleScreenReplay = !s2.singleScreenReplay;
  s2.spectatorWindow = !s2.spectatorWindow;
  s2.bloodParticleMax = 9999;
  auto hash3 = s2.updateHash();
  CHECK(hash1 == hash3);

  // Changing a gameplay field SHOULD change the hash
  s2.lives = 99;
  auto hash4 = s2.updateHash();
  CHECK(hash1 != hash4);

  // Changing another gameplay field
  Settings s3;
  s3.zoneTimeout = 999;
  auto hash5 = s3.updateHash();
  CHECK(hash1 != hash5);

  // Changing TC should change hash
  Settings s4;
  s4.tc = "different_tc";
  auto hash6 = s4.updateHash();
  CHECK(hash1 != hash6);
}

TEST_CASE("WormSettings profile round-trip") {
  // Set up a worm profile with non-default values
  WormSettings original;
  original.name = "TestWorm";
  original.randomName = false;
  original.controller = 1;
  original.health = 200;
  original.rgb[0] = 10;
  original.rgb[1] = 20;
  original.rgb[2] = 30;
  original.weapons[0] = 3;
  original.weapons[1] = 2;
  original.controlsEx[0] = 42;
  original.controlsEx[1] = 99;
  original.inputDevice = 1;
  original.gamepadName = "TestPad";
  original.gamepadSerial = "SERIAL123";
  original.gamepadControls[0] = 7;

  auto tmpPath =
      std::filesystem::temp_directory_path() / "openliero_test_profile.toml";

  // Save
  original.saveProfile(FsNode(tmpPath.string()));

  // Load into a fresh WormSettings
  WormSettings loaded;
  loaded.color = 55;  // Should be preserved
  loaded.loadProfile(FsNode(tmpPath.string()));

  CHECK(loaded.name == "TestWorm");
  CHECK(loaded.randomName == false);
  CHECK(loaded.controller == 1);
  CHECK(loaded.health == 200);
  CHECK(loaded.rgb[0] == 10);
  CHECK(loaded.rgb[1] == 20);
  CHECK(loaded.rgb[2] == 30);
  CHECK(loaded.weapons[0] == 3);
  CHECK(loaded.weapons[1] == 2);
  CHECK(loaded.controlsEx[0] == 42);
  CHECK(loaded.controlsEx[1] == 99);
  CHECK(loaded.inputDevice == 1);
  CHECK(loaded.gamepadName == "TestPad");
  CHECK(loaded.gamepadSerial == "SERIAL123");
  CHECK(loaded.gamepadControls[0] == 7);
  CHECK(loaded.color == 55);  // Color is preserved across profile load

  std::filesystem::remove(tmpPath);
}

TEST_CASE("Settings toToml/fromToml round-trip") {
  Settings original;
  original.maxBonuses = 8;
  original.blood = 150;
  original.lives = 30;
  original.fullscreen = true;
  original.tc = "custom_tc";
  original.wormSettings[0]->name = "Player1";
  original.wormSettings[0]->randomName = false;

  std::string toml = original.toToml();
  REQUIRE(!toml.empty());

  Settings loaded;
  loaded.fromToml(toml);

  CHECK(loaded.maxBonuses == 8);
  CHECK(loaded.blood == 150);
  CHECK(loaded.lives == 30);
  CHECK(loaded.fullscreen == true);
  CHECK(loaded.tc == "custom_tc");
  CHECK(loaded.wormSettings[0]->name == "Player1");
}

TEST_CASE("WormSettings toToml/fromToml round-trip") {
  WormSettings original;
  original.name = "TestWorm";
  original.randomName = false;
  original.health = 250;
  original.controller = 2;
  original.controlsEx[0] = 77;

  std::string toml = original.toToml();
  REQUIRE(!toml.empty());

  WormSettings loaded;
  loaded.fromToml(toml);

  CHECK(loaded.name == "TestWorm");
  CHECK(loaded.health == 250);
  CHECK(loaded.controller == 2);
  CHECK(loaded.controlsEx[0] == 77);
}
