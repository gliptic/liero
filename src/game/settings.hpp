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

  static bool const extensions = true;

  bool recordReplays;
  bool loadPowerlevelPalette;

  int aiFrames, aiMutations;
  bool aiTraces;
  int32_t aiParallels;

  int32_t zoneTimeout;
  uint32_t selectBotWeapons;

  bool allowViewingSpawnPoint;
  std::string tc;
};

// App/UI settings (not included in hash or replays)
struct AppSettings {
  AppSettings();

  bool fullscreen;
  bool singleScreenReplay;
  bool spectatorWindow;
  int32_t bloodParticleMax;
};

struct Rand;

struct Settings : GameplayExtensions, AppSettings {
  enum GameModes { GMKillEmAll, GMGameOfTag, GMHoldazone, GMScalesOfJustice, MaxGameModes };

  static int const selectableWeapons = 5;
  static int const zoneCaptureTime = 70;

  static int const wormAnimTab[];

  Settings();

  bool load(FsNode node, Rand& rand);
  void save(FsNode node, Rand& rand);
  std::string toToml() const;
  void fromToml(std::string const& data);
  uint64_t& updateHash();

  static void generateName(WormSettings& ws, Rand& rand);

  uint32_t weapTable[40];
  int32_t maxBonuses;
  int32_t blood;
  int32_t timeToLose;
  int32_t flagsToWin;
  uint32_t gameMode;
  bool shadow;
  bool loadChange;
  bool namesOnBonuses;
  bool regenerateLevel;
  int32_t lives;
  int32_t loadingTime;
  bool randomLevel;
  std::string levelFile;
  bool map;
  bool screenSync;
  int32_t bonusTimeout;  // max seconds a bonus stays on the map; 0 = no limit

  // Frames of artificial input delay. Host-authoritative; synced to
  // the client via MatchSettingsData.
  int32_t inputDelay;

  static int const NumWormSettings = 3;  // 0=left, 1=right, 2=network
  static int const NetworkPlayerIdx = 2;
  static int const ConfigVersion = 3;  // bump when adding fields to the TOML config
  std::shared_ptr<WormSettings> wormSettings[NumWormSettings];

  uint64_t hash;
};

template <int L, int H, typename T>
inline T limit(T v) {
  if (v >= (T)H)
    return (T)H - 1;
  else if (v < (T)L)
    return (T)L;

  return v;
}
