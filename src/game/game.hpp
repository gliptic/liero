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
  StateInitial,
  StateWeaponSelection,
  StateGame,
  StateGameEnded,
} GameState;

struct Holdazone {
  Holdazone()
      : holderIdx(-1),
        contenderIdx(-1),
        contenderFrames(0),
        timeoutLeft(0),
        zoneWidth(50),
        zoneHeight(34) {}

  Rect rect;
  int holderIdx;

  int contenderIdx, contenderFrames;

  int timeoutLeft;

  int zoneWidth, zoneHeight;
};

struct Game {
  Game(std::shared_ptr<Common> common, std::shared_ptr<Settings> settings,
       std::shared_ptr<SoundPlayer> soundPlayer);
  ~Game();

  void onKey(uint32_t key, bool state);
  Worm* findControlForKey(uint32_t key, Worm::Control& control);
  void releaseControls();
  void processFrame();
  void focus(Renderer& renderer);
  void updateSettings(Renderer& renderer);

  void createBObject(fixedvec pos, fixedvec vel);
  void createBonus();

  void clearViewports();
  void addViewport(Viewport*);
  void addSpectatorViewport(SpectatorViewport*);
  void processViewports();
  void drawViewports(Renderer& renderer, GameState state, bool isReplay = false);
  void drawSpectatorViewports(Renderer& renderer, GameState state, bool isReplay = false);
  void clearWorms();
  void addWorm(std::shared_ptr<Worm>);
  void resetWorms();
  void draw(Renderer& renderer, GameState state, bool useSpectatorViewports, bool isReplay = false);
  void startGame();
  bool isGameOver();
  void doDamageDirect(Worm& w, int amount, int byIdx);
  void doHealingDirect(Worm& w, int amount);
  void doDamage(Worm& w, int amount, int byIdx);
  void doHealing(Worm& w, int amount);
  void postClone(Game& original, bool complete = false);

  // Full mid-game state snapshot (cereal-based). Round-trips every
  // sim-affecting field; correctness oracle for the fast path.
  void saveSnapshot(std::vector<uint8_t>& out) const;
  void loadSnapshot(std::vector<uint8_t> const& in);

  // Fast in-memory snapshot path used by the rollback ring buffer.
  // Writes/reads directly into a pre-allocated GameSnapshot — no
  // serialisation, no allocation in the steady state.
  void saveSnapshotFast(struct GameSnapshot& out) const;
  void loadSnapshotFast(struct GameSnapshot const& in);

  void spawnZone();

  // While speculative is true, sim-driven side effects
  // (SoundPlayer::play/stop, StatsRecorder writes) are suppressed.
  // Set during predicted frames and during rollback resim.
  void setSpeculative(bool s) {
    speculative = s;
    if (soundPlayer) soundPlayer->speculative = s;
    if (statsRecorder) statsRecorder->speculative = s;
  }

  Material pixelMat(int x, int y) { return common->materials[level.pixel(x, y)]; }

  Worm* wormByIdx(int idx) {
    if (idx < 0) return 0;
    return worms[idx].get();
  }

  std::shared_ptr<Common> common;
  std::shared_ptr<SoundPlayer> soundPlayer;
  SoundPlayer* prevSoundPlayer;
  bool soundPlayerInstalled;
  std::shared_ptr<Settings> settings;
  std::shared_ptr<StatsRecorder> statsRecorder;

  Level level;

  int screenFlash;
  bool gotChanged;
  int lastKilledIdx;
  bool paused;
  int cycles;
  Rand rand;

  Holdazone holdazone;

  std::vector<Viewport*> viewports;
  std::vector<SpectatorViewport*> spectatorViewports;
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

  bool quickSim;

  // True during predicted/resim frames. Mirrored onto soundPlayer /
  // statsRecorder via setSpeculative(). Read by Game-internal code
  // that short-circuits a side effect at its source.
  bool speculative = false;
};

bool checkRespawnPosition(Game& game, int x2, int y2, int oldX, int oldY, int x, int y);

// Checksum of game state for desync detection. Folds in RNG state,
// worm pos/vel/health/lives/ammo/aim, projectile pools, level damage,
// and control state so visible-but-silent divergences (e.g. mid-air
// projectile drift with worm position still matching) trip the
// receiver's checksum comparison instead of going undetected. Used
// by both the rollback controller and the replay format.
uint32_t wideRollbackChecksum(Game& game);
