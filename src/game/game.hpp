#pragma once

#include <string>
#include <vector>
#include "bobject.hpp"
#include "bonus.hpp"
#include "common.hpp"
#include "constants.hpp"
#include "level.hpp"
#include "mixer/player.hpp"
#include "nobject.hpp"
#include "rand.hpp"
#include "settings.hpp"
#include "sobject.hpp"
#include "stats_recorder.hpp"
#include "weapon.hpp"

struct SpectatorViewport;
struct Viewport;
struct Worm;
struct Renderer;

typedef enum {
  kStateInitial,
  kStateWeaponSelection,
  kStateGame,
  kStateGameEnded,
} GameState;

struct Holdazone {
  Holdazone()
      : holder_idx(-1),
        contender_idx(-1),
        contender_frames(0),
        timeout_left(0),
        zone_width(50),
        zone_height(34) {}

  Rect rect;
  int holder_idx;

  int contender_idx, contender_frames;

  int timeout_left;

  int zone_width, zone_height;
};

struct Game {
  Game(std::shared_ptr<Common> common, std::shared_ptr<Settings> settings,
       std::shared_ptr<SoundPlayer> sound_player);
  ~Game();

  void OnKey(uint32_t key, bool state);
  Worm* FindControlForKey(uint32_t key, Worm::Control& control);
  void ReleaseControls();
  void ProcessFrame();
  void Focus(Renderer& renderer);
  void UpdateSettings(Renderer& renderer);

  void CreateBObject(fixedvec pos, fixedvec vel);
  void CreateBonus();

  void ClearViewports();
  void AddViewport(Viewport*);
  void AddSpectatorViewport(SpectatorViewport*);
  void ProcessViewports();
  void DrawViewports(Renderer& renderer, GameState state, bool is_replay = false);
  void DrawSpectatorViewports(Renderer& renderer, GameState state, bool is_replay = false);
  void ClearWorms();
  void AddWorm(std::shared_ptr<Worm>);
  void ResetWorms();
  void Draw(Renderer& renderer, GameState state, bool use_spectator_viewports,
            bool is_replay = false);
  void StartGame();
  bool IsGameOver();
  void DoDamageDirect(Worm& w, int amount, int by_idx);
  void DoHealingDirect(Worm& w, int amount);
  void DoDamage(Worm& w, int amount, int by_idx);
  void DoHealing(Worm& w, int amount);
  void PostClone(Game& original, bool complete = false);

  // Full mid-game state snapshot (cereal-based). Round-trips every
  // sim-affecting field; correctness oracle for the fast path.
  void SaveSnapshot(std::vector<uint8_t>& out) const;
  void LoadSnapshot(std::vector<uint8_t> const& in);

  // Fast in-memory snapshot path used by the rollback ring buffer.
  // Writes/reads directly into a pre-allocated GameSnapshot — no
  // serialisation, no allocation in the steady state.
  void SaveSnapshotFast(struct GameSnapshot& out) const;
  void LoadSnapshotFast(struct GameSnapshot const& in);

  void SpawnZone();

  // While speculative is true, sim-driven side effects
  // (SoundPlayer::play/stop, StatsRecorder writes) are suppressed.
  // Set during predicted frames and during rollback resim.
  void SetSpeculative(bool s) {
    speculative = s;
    if (sound_player) sound_player->speculative = s;
    if (stats_recorder) stats_recorder->speculative = s;
  }

  Material PixelMat(int x, int y) { return common->materials[level.Pixel(x, y)]; }

  Worm* WormByIdx(int idx) {
    if (idx < 0) return 0;
    return worms[idx].get();
  }

  std::shared_ptr<Common> common;
  std::shared_ptr<SoundPlayer> sound_player;
  SoundPlayer* prev_sound_player;
  bool sound_player_installed;
  std::shared_ptr<Settings> settings;
  std::shared_ptr<StatsRecorder> stats_recorder;

  Level level;

  int screen_flash;
  bool got_changed;
  int last_killed_idx;
  bool paused;
  int cycles;
  Rand rand;

  Holdazone holdazone;

  std::vector<Viewport*> viewports;
  std::vector<SpectatorViewport*> spectator_viewports;
  std::vector<std::shared_ptr<Worm>> worms;

  typedef ExactObjectList<Bonus, 99> BonusList;
  typedef ExactObjectList<WObject, 600> WObjectList;
  typedef ExactObjectList<SObject, 700> SObjectList;
  typedef ExactObjectList<NObject, 600> NObjectList;
  typedef FastObjectList<BObject> BObjectList;
  BonusList bonuses;
  WObjectList wobjects;
  SObjectList sobjects;
  NObjectList nobjects;
  BObjectList bobjects;

  bool quick_sim;

  // True during predicted/resim frames. Mirrored onto soundPlayer /
  // statsRecorder via setSpeculative(). Read by Game-internal code
  // that short-circuits a side effect at its source.
  bool speculative = false;
};

bool CheckRespawnPosition(Game& game, int x2, int y2, int old_x, int old_y, int x, int y);

// Checksum of game state for desync detection. Folds in RNG state,
// worm pos/vel/health/lives/ammo/aim, projectile pools, level damage,
// and control state so visible-but-silent divergences (e.g. mid-air
// projectile drift with worm position still matching) trip the
// receiver's checksum comparison instead of going undetected. Used
// by both the rollback controller and the replay format.
uint32_t WideRollbackChecksum(Game& game);
