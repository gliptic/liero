#pragma once

#include <xxhash.h>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include "worm.hpp"

// Gameplay-affecting settings (included in hash computation and replays)
struct GameplayExtensions {
  GameplayExtensions();

  static bool const kExtensions = true;

  bool record_replays;
  bool load_powerlevel_palette;

  int ai_frames, ai_mutations;
  bool ai_traces;
  int32_t ai_parallels;

  int32_t zone_timeout;
  uint32_t select_bot_weapons;

  bool allow_viewing_spawn_point;
  std::string tc;
};

// App/UI settings (not included in hash or replays)
struct AppSettings {
  AppSettings();

  bool fullscreen;
  bool single_screen_replay;
  bool spectator_window;
  int32_t blood_particle_max;
};

struct Rand;

struct Settings : GameplayExtensions, AppSettings {
  enum GameModes { kGmKillEmAll, kGmGameOfTag, kGmHoldazone, kGmScalesOfJustice, kMaxGameModes };

  static int const kSelectableWeapons = 5;
  static int const kZoneCaptureTime = 70;

  static int const kWormAnimTab[];

  Settings();

  bool load(FsNode node, Rand& rand);
  void save(FsNode node, Rand& rand);
  std::string ToToml() const;
  void FromToml(std::string const& data);
  uint64_t& UpdateHash();

  static void GenerateName(WormSettings& ws, Rand& rand);

  uint32_t weap_table[40];
  int32_t max_bonuses;
  int32_t blood;
  int32_t time_to_lose;
  int32_t flags_to_win;
  uint32_t game_mode;
  bool shadow;
  bool load_change;
  bool names_on_bonuses;
  bool regenerate_level;
  int32_t lives;
  int32_t loading_time;
  bool random_level;
  std::string level_file;
  bool map;
  bool screen_sync;
  int32_t bonus_timeout;  // max seconds a bonus stays on the map; 0 = no limit

  // Frames of artificial input delay. Host-authoritative; synced to
  // the client via MatchSettingsData.
  int32_t input_delay;

  static int const kNumWormSettings = 3;  // 0=left, 1=right, 2=network
  static int const kNetworkPlayerIdx = 2;
  static int const kConfigVersion = 3;  // bump when adding fields to the TOML config
  std::shared_ptr<WormSettings> worm_settings[kNumWormSettings];

  uint64_t hash;
};

template <int L, int H, typename T>
inline T Limit(T v) {
  if (v >= (T)H)
    return (T)H - 1;
  else if (v < (T)L)
    return (T)L;

  return v;
}
