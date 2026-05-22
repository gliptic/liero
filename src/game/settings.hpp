#pragma once

#include <cstdint>
#include <cstring>
#include <gvl/crypt/gash.hpp>
#include <gvl/serialization/archive.hpp>
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

struct Settings : gvl::shared, GameplayExtensions, AppSettings {
  enum GameModes {
    GMKillEmAll,
    GMGameOfTag,
    GMHoldazone,
    GMScalesOfJustice,
    MaxGameModes
  };

  static int const selectableWeapons = 5;
  static int const zoneCaptureTime = 70;

  static int const wormAnimTab[];

  Settings();

  bool load(FsNode node, Rand& rand);
  void save(FsNode node, Rand& rand);
  std::string toToml() const;
  void fromToml(std::string const& data);
  gvl::gash::value_type& updateHash();

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

  static int const NumWormSettings = 3;  // 0=left, 1=right, 2=network
  static int const NetworkPlayerIdx = 2;
  std::shared_ptr<WormSettings> wormSettings[NumWormSettings];

  gvl::gash::value_type hash;
};

template <int L, int H, typename T>
inline T limit(T v) {
  if (v >= (T)H)
    return (T)H - 1;
  else if (v < (T)L)
    return (T)L;

  return v;
}

// Settings archive for replays: embeds TOML as a string in the binary stream.
template <typename Archive>
void archive(Archive ar, Settings& settings) {
  if (ar.out) {
    std::string toml = settings.toToml();
    ar.str(toml);
  } else {
    std::string toml;
    ar.str(toml);
    settings.fromToml(toml);
  }
  ar.check();
}

// Serialize only gameplay-affecting fields (used for hash computation).
// This ensures that changing UI-only settings doesn't affect the hash.
template <typename Archive>
void archive_gameplay_text(Settings& settings, Archive& ar) {
#define S(n) #n, settings.n

  ar.i32(S(maxBonuses));
  ar.i32(S(loadingTime));
  ar.i32(S(lives));
  ar.i32(S(timeToLose));
  ar.i32(S(flagsToWin));
  ar.b(S(screenSync));
  ar.b(S(map));
  ar.b(S(randomLevel));
  ar.i32(S(blood));
  ar.u32(S(gameMode));
  ar.b(S(namesOnBonuses));
  ar.b(S(regenerateLevel));
  ar.b(S(shadow));
  ar.b(S(loadChange));
  ar.str(S(levelFile));

  ar.b(S(recordReplays));
  ar.b(S(loadPowerlevelPalette));

  ar.i32(S(aiMutations))
      .i32(S(aiFrames))
      .u32(S(selectBotWeapons))
      .i32(S(zoneTimeout));

  ar.b(S(aiTraces)).i32(S(aiParallels));
  ar.b(S(allowViewingSpawnPoint));
  ar.str(S(tc));

#undef S

  ar.arr("weapTable", settings.weapTable, [&](uint32_t& v) {
    ar.u32(0, v);
    if (ar.in)
      v = limit<0, 3>(v);
  });
}

template <typename Archive>
void archive_text(Settings& settings, Archive& ar) {
  // TODO: Manage defaults when it becomes necessary

#define S(n) #n, settings.n

  ar.i32(S(maxBonuses));
  ar.i32(S(loadingTime));
  ar.i32(S(lives));
  ar.i32(S(timeToLose));
  ar.i32(S(flagsToWin));
  ar.b(S(screenSync));
  ar.b(S(map));
  ar.b(S(randomLevel));
  ar.i32(S(blood));
  ar.u32(S(gameMode));
  ar.b(S(namesOnBonuses));
  ar.b(S(regenerateLevel));
  ar.b(S(shadow));
  ar.b(S(loadChange));
  ar.str(S(levelFile));

  ar.b(S(recordReplays));
  ar.b(S(loadPowerlevelPalette));

  ar.i32(S(aiMutations))
      .i32(S(aiFrames))
      .u32(S(selectBotWeapons))
      .i32(S(zoneTimeout));

  ar.b(S(aiTraces)).i32(S(aiParallels));

  ar.b(S(allowViewingSpawnPoint));
  ar.b(S(singleScreenReplay));
  ar.b(S(spectatorWindow));
  ar.b(S(fullscreen));
  ar.str(S(tc));
  ar.i32(S(bloodParticleMax));

#undef S

  ar.arr("weapTable", settings.weapTable, [&](uint32_t& v) {
    ar.u32(0, v);
    if (ar.in)
      v = limit<0, 3>(v);
  });

  // Serialize the first 2 worms (left/right players) as the "worms" array
  std::shared_ptr<WormSettings> twoWorms[2] = {
      settings.wormSettings[0], settings.wormSettings[1]};
  ar.array_obj(
      "worms", twoWorms,
      [&](std::shared_ptr<WormSettings> const& ws) {
        archive_worm_toml(ar, *ws);
      });
  if (ar.in) {
    settings.wormSettings[0] = twoWorms[0];
    settings.wormSettings[1] = twoWorms[1];
  }

  // Serialize network player as a separate [netPlayer] object
  {
    auto& ws = settings.wormSettings[Settings::NetworkPlayerIdx];
    ar.obj("netPlayer", [&] {
      archive_worm_toml(ar, *ws);
    });
  }
}
