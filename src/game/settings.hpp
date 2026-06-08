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

  bool record_replays{true};
  bool load_powerlevel_palette{true};

  int ai_frames{70 * 2}, ai_mutations{2};
  bool ai_traces{false};
  int32_t ai_parallels{3};

  int32_t zone_timeout{30};
  uint32_t select_bot_weapons{true};

  bool allow_viewing_spawn_point{false};
  std::string tc;
};

// App/UI settings (not included in hash or replays)
struct AppSettings {
  AppSettings();

  bool fullscreen{false};
  bool single_screen_replay{false};
  bool spectator_window{false};
  int32_t blood_particle_max{700};
};

struct Rand;

struct Settings : GameplayExtensions, AppSettings {
  enum GameModes { kGmKillEmAll, kGmGameOfTag, kGmHoldazone, kGmScalesOfJustice, kMaxGameModes };

  static int const kSelectableWeapons = 5;
  static int const kZoneCaptureTime = 70;

  static int const kWormAnimTab[];

  Settings();

  bool load(const FsNode& node, Rand& rand);
  void save(const FsNode& node, Rand& rand) const;
  std::string ToToml() const;
  void FromToml(std::string const& data);
  uint64_t& UpdateHash();

  static void GenerateName(WormSettings& ws, Rand& rand);

  uint32_t weap_table[40];
  int32_t max_bonuses{4};
  int32_t blood{100};
  int32_t time_to_lose{600};
  int32_t flags_to_win{20};
  uint32_t game_mode{0};
  bool shadow{true};
  bool load_change{true};
  bool names_on_bonuses{false};
  bool regenerate_level{false};
  int32_t lives{15};
  int32_t loading_time{100};
  bool random_level{true};
  std::string level_file;
  bool map{true};
  bool screen_sync{true};
  int32_t bonus_timeout{0};  // max seconds a bonus stays on the map; 0 = no limit

  // Frames of artificial input delay. Host-authoritative; synced to
  // the client via MatchSettingsData.
  int32_t input_delay{1};

  static int const kNumWormSettings = 3;  // 0=left, 1=right, 2=network
  static int const kNetworkPlayerIdx = 2;
  static int const kConfigVersion = 3;  // bump when adding fields to the TOML config
  std::shared_ptr<WormSettings> worm_settings[kNumWormSettings];

  uint64_t hash;
};

template <int L, int H, typename T>
inline T Limit(T v) {
  if (v >= static_cast<T>(H)) return static_cast<T>(H) - 1;
  if (v < static_cast<T>(L)) return static_cast<T>(L);

  return v;
}
