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
  rand.Seed(42);

  // Create settings with non-default values
  Settings original;
  original.max_bonuses = 7;
  original.blood = 200;
  original.time_to_lose = 300;
  original.flags_to_win = 10;
  original.game_mode = 2;
  original.shadow = false;
  original.load_change = false;
  original.names_on_bonuses = true;
  original.regenerate_level = true;
  original.lives = 25;
  original.loading_time = 50;
  original.random_level = false;
  original.level_file = "testlevel.lev";
  original.map = false;
  original.screen_sync = false;

  // Extension fields
  original.record_replays = false;
  original.load_powerlevel_palette = false;
  original.blood_particle_max = 500;
  original.ai_frames = 200;
  original.ai_mutations = 5;
  original.ai_traces = true;
  original.ai_parallels = 8;
  original.fullscreen = true;
  original.zone_timeout = 60;
  original.select_bot_weapons = 2;
  original.allow_viewing_spawn_point = true;
  original.single_screen_replay = true;
  original.spectator_window = true;
  original.tc = "customtc";

  // Weapon table
  for (int i = 0; i < 40; ++i) original.weap_table[i] = i % 3;

  // Worm settings (player 0)
  original.worm_settings[0]->controller = 1;
  original.worm_settings[0]->health = 150;
  original.worm_settings[0]->name = "TestWorm1";
  original.worm_settings[0]->random_name = false;
  original.worm_settings[0]->rgb[0] = 10;
  original.worm_settings[0]->rgb[1] = 20;
  original.worm_settings[0]->rgb[2] = 30;
  original.worm_settings[0]->controls_ex[0] = 100;
  original.worm_settings[0]->controls_ex[1] = 101;
  original.worm_settings[0]->weapons[0] = 5;
  original.worm_settings[0]->weapons[1] = 10;
  original.worm_settings[0]->input_device = 1;
  original.worm_settings[0]->gamepad_name = "Xbox Controller";

  // Worm settings (player 1)
  original.worm_settings[1]->controller = 2;
  original.worm_settings[1]->health = 200;
  original.worm_settings[1]->name = "TestWorm2";
  original.worm_settings[1]->random_name = false;
  original.worm_settings[1]->rgb[0] = 40;
  original.worm_settings[1]->rgb[1] = 50;
  original.worm_settings[1]->rgb[2] = 60;
  original.worm_settings[1]->controls_ex[2] = 200;

  // Network player (player 2)
  original.worm_settings[2]->controller = 1;
  original.worm_settings[2]->health = 175;
  original.worm_settings[2]->name = "NetPlayer";
  original.worm_settings[2]->random_name = false;
  original.worm_settings[2]->rgb[0] = 5;
  original.worm_settings[2]->rgb[1] = 15;
  original.worm_settings[2]->rgb[2] = 25;
  original.worm_settings[2]->controls_ex[0] = 50;
  original.worm_settings[2]->controls_ex[3] = 77;
  original.worm_settings[2]->weapons[0] = 3;
  original.worm_settings[2]->weapons[2] = 7;
  original.worm_settings[2]->input_device = 2;
  original.worm_settings[2]->gamepad_name = "PS5 Controller";
  original.worm_settings[2]->gamepad_serial = "ABC123";
  original.worm_settings[2]->gamepad_controls[0] = 10;

  // Save to temp file
  auto tmp_path = std::filesystem::temp_directory_path() / "openliero_test_settings.cfg";
  FsNode const kNode(tmp_path.string());

  original.save(kNode, rand);

  // Load into a fresh Settings object
  Settings loaded;
  REQUIRE(loaded.load(kNode, rand));

  // Verify base settings
  CHECK(loaded.max_bonuses == original.max_bonuses);
  CHECK(loaded.blood == original.blood);
  CHECK(loaded.time_to_lose == original.time_to_lose);
  CHECK(loaded.flags_to_win == original.flags_to_win);
  CHECK(loaded.game_mode == original.game_mode);
  CHECK(loaded.shadow == original.shadow);
  CHECK(loaded.load_change == original.load_change);
  CHECK(loaded.names_on_bonuses == original.names_on_bonuses);
  CHECK(loaded.regenerate_level == original.regenerate_level);
  CHECK(loaded.lives == original.lives);
  CHECK(loaded.loading_time == original.loading_time);
  CHECK(loaded.random_level == original.random_level);
  CHECK(loaded.level_file == original.level_file);
  CHECK(loaded.map == original.map);
  CHECK(loaded.screen_sync == original.screen_sync);

  // Extension fields
  CHECK(loaded.record_replays == original.record_replays);
  CHECK(loaded.load_powerlevel_palette == original.load_powerlevel_palette);
  CHECK(loaded.blood_particle_max == original.blood_particle_max);
  CHECK(loaded.ai_frames == original.ai_frames);
  CHECK(loaded.ai_mutations == original.ai_mutations);
  CHECK(loaded.ai_traces == original.ai_traces);
  CHECK(loaded.ai_parallels == original.ai_parallels);
  CHECK(loaded.fullscreen == original.fullscreen);
  CHECK(loaded.zone_timeout == original.zone_timeout);
  CHECK(loaded.select_bot_weapons == original.select_bot_weapons);
  CHECK(loaded.allow_viewing_spawn_point == original.allow_viewing_spawn_point);
  CHECK(loaded.single_screen_replay == original.single_screen_replay);
  CHECK(loaded.spectator_window == original.spectator_window);
  CHECK(loaded.tc == original.tc);

  // Weapon table
  for (int i = 0; i < 40; ++i) CHECK(loaded.weap_table[i] == original.weap_table[i]);

  // Player 0
  CHECK(loaded.worm_settings[0]->controller == original.worm_settings[0]->controller);
  CHECK(loaded.worm_settings[0]->health == original.worm_settings[0]->health);
  CHECK(loaded.worm_settings[0]->name == original.worm_settings[0]->name);
  CHECK(loaded.worm_settings[0]->rgb[0] == original.worm_settings[0]->rgb[0]);
  CHECK(loaded.worm_settings[0]->rgb[1] == original.worm_settings[0]->rgb[1]);
  CHECK(loaded.worm_settings[0]->rgb[2] == original.worm_settings[0]->rgb[2]);
  CHECK(loaded.worm_settings[0]->controls_ex[0] == original.worm_settings[0]->controls_ex[0]);
  CHECK(loaded.worm_settings[0]->controls_ex[1] == original.worm_settings[0]->controls_ex[1]);
  CHECK(loaded.worm_settings[0]->weapons[0] == original.worm_settings[0]->weapons[0]);
  CHECK(loaded.worm_settings[0]->weapons[1] == original.worm_settings[0]->weapons[1]);
  CHECK(loaded.worm_settings[0]->input_device == original.worm_settings[0]->input_device);
  CHECK(loaded.worm_settings[0]->gamepad_name == original.worm_settings[0]->gamepad_name);

  // Player 1
  CHECK(loaded.worm_settings[1]->controller == original.worm_settings[1]->controller);
  CHECK(loaded.worm_settings[1]->health == original.worm_settings[1]->health);
  CHECK(loaded.worm_settings[1]->name == original.worm_settings[1]->name);
  CHECK(loaded.worm_settings[1]->rgb[0] == original.worm_settings[1]->rgb[0]);
  CHECK(loaded.worm_settings[1]->controls_ex[2] == original.worm_settings[1]->controls_ex[2]);

  // Network player (the critical one that was broken)
  CHECK(loaded.worm_settings[2]->controller == original.worm_settings[2]->controller);
  CHECK(loaded.worm_settings[2]->health == original.worm_settings[2]->health);
  CHECK(loaded.worm_settings[2]->name == original.worm_settings[2]->name);
  CHECK(loaded.worm_settings[2]->rgb[0] == original.worm_settings[2]->rgb[0]);
  CHECK(loaded.worm_settings[2]->rgb[1] == original.worm_settings[2]->rgb[1]);
  CHECK(loaded.worm_settings[2]->rgb[2] == original.worm_settings[2]->rgb[2]);
  CHECK(loaded.worm_settings[2]->controls_ex[0] == original.worm_settings[2]->controls_ex[0]);
  CHECK(loaded.worm_settings[2]->controls_ex[3] == original.worm_settings[2]->controls_ex[3]);
  CHECK(loaded.worm_settings[2]->weapons[0] == original.worm_settings[2]->weapons[0]);
  CHECK(loaded.worm_settings[2]->weapons[2] == original.worm_settings[2]->weapons[2]);
  CHECK(loaded.worm_settings[2]->input_device == original.worm_settings[2]->input_device);
  CHECK(loaded.worm_settings[2]->gamepad_name == original.worm_settings[2]->gamepad_name);
  CHECK(loaded.worm_settings[2]->gamepad_serial == original.worm_settings[2]->gamepad_serial);
  CHECK(loaded.worm_settings[2]->gamepad_controls[0] ==
        original.worm_settings[2]->gamepad_controls[0]);

  // Cleanup
  std::filesystem::remove(tmp_path);
}

TEST_CASE("Settings load handles missing file gracefully") {
  Rand rand;
  rand.Seed(1);

  Settings settings;
  FsNode const kNode("/tmp/nonexistent_openliero_settings.cfg");
  CHECK_FALSE(settings.load(kNode, rand));
}

TEST_CASE("Settings TOML with comments") {
  Rand rand;
  rand.Seed(42);

  // Write using toToml(), then verify it can be re-loaded
  Settings original;
  original.max_bonuses = 12;
  original.blood = 50;

  auto tmp_path = std::filesystem::temp_directory_path() / "openliero_test_comments.cfg";

  {
    std::ofstream f(tmp_path);
    f << "# This is a comment\n";
    f << original.ToToml();
  }

  FsNode const kNode(tmp_path.string());
  Settings loaded;
  REQUIRE(loaded.load(kNode, rand));
  std::filesystem::remove(tmp_path);

  CHECK(loaded.max_bonuses == 12);
  CHECK(loaded.blood == 50);
}

TEST_CASE("Settings hash only includes gameplay fields") {
  Settings s1;
  Settings s2;

  // Both should have the same hash initially
  auto hash1 = s1.UpdateHash();
  auto hash2 = s2.UpdateHash();
  CHECK(hash1 == hash2);

  // Changing an app-only field should NOT change the hash
  s2.fullscreen = !s2.fullscreen;
  s2.single_screen_replay = !s2.single_screen_replay;
  s2.spectator_window = !s2.spectator_window;
  s2.blood_particle_max = 9999;
  auto hash3 = s2.UpdateHash();
  CHECK(hash1 == hash3);

  // Changing a gameplay field SHOULD change the hash
  s2.lives = 99;
  auto hash4 = s2.UpdateHash();
  CHECK(hash1 != hash4);

  // Changing another gameplay field
  Settings s3;
  s3.zone_timeout = 999;
  auto hash5 = s3.UpdateHash();
  CHECK(hash1 != hash5);

  // Changing TC should change hash
  Settings s4;
  s4.tc = "different_tc";
  auto hash6 = s4.UpdateHash();
  CHECK(hash1 != hash6);
}

TEST_CASE("WormSettings profile round-trip") {
  // Set up a worm profile with non-default values
  WormSettings original;
  original.name = "TestWorm";
  original.random_name = false;
  original.controller = 1;
  original.health = 200;
  original.rgb[0] = 10;
  original.rgb[1] = 20;
  original.rgb[2] = 30;
  original.weapons[0] = 3;
  original.weapons[1] = 2;
  original.controls_ex[0] = 42;
  original.controls_ex[1] = 99;
  original.input_device = 1;
  original.gamepad_name = "TestPad";
  original.gamepad_serial = "SERIAL123";
  original.gamepad_controls[0] = 7;

  auto tmp_path = std::filesystem::temp_directory_path() / "openliero_test_profile.toml";

  // Save
  original.SaveProfile(FsNode(tmp_path.string()));

  // Load into a fresh WormSettings
  WormSettings loaded;
  loaded.color = 55;  // Should be preserved
  loaded.LoadProfile(FsNode(tmp_path.string()));

  CHECK(loaded.name == "TestWorm");
  CHECK(loaded.random_name == false);
  CHECK(loaded.controller == 1);
  CHECK(loaded.health == 200);
  CHECK(loaded.rgb[0] == 10);
  CHECK(loaded.rgb[1] == 20);
  CHECK(loaded.rgb[2] == 30);
  CHECK(loaded.weapons[0] == 3);
  CHECK(loaded.weapons[1] == 2);
  CHECK(loaded.controls_ex[0] == 42);
  CHECK(loaded.controls_ex[1] == 99);
  CHECK(loaded.input_device == 1);
  CHECK(loaded.gamepad_name == "TestPad");
  CHECK(loaded.gamepad_serial == "SERIAL123");
  CHECK(loaded.gamepad_controls[0] == 7);
  CHECK(loaded.color == 55);  // Color is preserved across profile load

  std::filesystem::remove(tmp_path);
}

TEST_CASE("Settings toToml/fromToml round-trip") {
  Settings original;
  original.max_bonuses = 8;
  original.blood = 150;
  original.lives = 30;
  original.fullscreen = true;
  original.tc = "custom_tc";
  original.worm_settings[0]->name = "Player1";
  original.worm_settings[0]->random_name = false;

  std::string const kToml = original.ToToml();
  REQUIRE(!kToml.empty());

  Settings loaded;
  loaded.FromToml(kToml);

  CHECK(loaded.max_bonuses == 8);
  CHECK(loaded.blood == 150);
  CHECK(loaded.lives == 30);
  CHECK(loaded.fullscreen == true);
  CHECK(loaded.tc == "custom_tc");
  CHECK(loaded.worm_settings[0]->name == "Player1");
}

TEST_CASE("WormSettings toToml/fromToml round-trip") {
  WormSettings original;
  original.name = "TestWorm";
  original.random_name = false;
  original.health = 250;
  original.controller = 2;
  original.controls_ex[0] = 77;

  std::string const kToml = original.ToToml();
  REQUIRE(!kToml.empty());

  WormSettings loaded;
  loaded.FromToml(kToml);

  CHECK(loaded.name == "TestWorm");
  CHECK(loaded.health == 250);
  CHECK(loaded.controller == 2);
  CHECK(loaded.controls_ex[0] == 77);
}
