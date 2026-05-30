#pragma once

// Fast in-memory snapshot path used by the rollback ring buffer. Stores
// sim state directly in a GameSnapshot struct kept resident next to the
// live Game. The cereal snapshot in snapshot.hpp stays as the correctness
// oracle (see test_snapshot_fast.cpp).
//
// Worm is copied via WormSimState rather than memcpy because Worm holds
// shared_ptrs; the raw Worm*/Weapon const* pointers inside Ninjarope and
// WormWeapon are stable across the rollback window (their targets are
// owned by Game / Common) so plain assignment is correct. Object pools
// and Level vectors are pure POD blocks.

#include "bobject.hpp"
#include "bonus.hpp"
#include "game.hpp"
#include "material.hpp"
#include "nobject.hpp"
#include "rand.hpp"
#include "sobject.hpp"
#include "weapon.hpp"
#include "worm.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

struct WormSimState {
  fixedvec pos{}, vel{};
  IVec2 logicRespawn{};
  int hotspotX = 0, hotspotY = 0;
  fixed aimingAngle = 0, aimingSpeed = 0;
  bool ableToJump = false, ableToDig = false;
  bool keyChangePressed = false;
  bool movable = false;
  bool animate = false, visible = false, ready = false, flag = false;
  bool makeSightGreen = false;
  int health = 0, lives = 0, kills = 0;
  int timer = 0, killedTimer = 0;
  int currentFrame = 0;
  int flags = 0;
  Ninjarope ninjarope;
  int currentWeapon = 0;
  int lastKilledByIdx = -1;
  int fireCone = 0;
  int leaveShellTimer = 0;
  int reacts[4] = {0, 0, 0, 0};
  WormWeapon weapons[NUM_WEAPONS];
  int direction = 0;
  Worm::ControlState controlStates, prevControlStates;
  int steerableSumX = 0, steerableSumY = 0, steerableCount = 0;
  int index = 0;
};

inline void saveWormSimState(WormSimState& s, Worm const& w) {
  s.pos = w.pos;
  s.vel = w.vel;
  s.logicRespawn = w.logicRespawn;
  s.hotspotX = w.hotspotX;
  s.hotspotY = w.hotspotY;
  s.aimingAngle = w.aimingAngle;
  s.aimingSpeed = w.aimingSpeed;
  s.ableToJump = w.ableToJump;
  s.ableToDig = w.ableToDig;
  s.keyChangePressed = w.keyChangePressed;
  s.movable = w.movable;
  s.animate = w.animate;
  s.visible = w.visible;
  s.ready = w.ready;
  s.flag = w.flag;
  s.makeSightGreen = w.makeSightGreen;
  s.health = w.health;
  s.lives = w.lives;
  s.kills = w.kills;
  s.timer = w.timer;
  s.killedTimer = w.killedTimer;
  s.currentFrame = w.currentFrame;
  s.flags = w.flags;
  s.ninjarope = w.ninjarope;
  s.currentWeapon = w.currentWeapon;
  s.lastKilledByIdx = w.lastKilledByIdx;
  s.fireCone = w.fireCone;
  s.leaveShellTimer = w.leaveShellTimer;
  std::memcpy(s.reacts, w.reacts, sizeof(s.reacts));
  for (int i = 0; i < NUM_WEAPONS; ++i) s.weapons[i] = w.weapons[i];
  s.direction = w.direction;
  s.controlStates = w.controlStates;
  s.prevControlStates = w.prevControlStates;
  s.steerableSumX = w.steerableSumX;
  s.steerableSumY = w.steerableSumY;
  s.steerableCount = w.steerableCount;
  s.index = w.index;
}

inline void restoreWormSimState(Worm& w, WormSimState const& s) {
  w.pos = s.pos;
  w.vel = s.vel;
  w.logicRespawn = s.logicRespawn;
  w.hotspotX = s.hotspotX;
  w.hotspotY = s.hotspotY;
  w.aimingAngle = s.aimingAngle;
  w.aimingSpeed = s.aimingSpeed;
  w.ableToJump = s.ableToJump;
  w.ableToDig = s.ableToDig;
  w.keyChangePressed = s.keyChangePressed;
  w.movable = s.movable;
  w.animate = s.animate;
  w.visible = s.visible;
  w.ready = s.ready;
  w.flag = s.flag;
  w.makeSightGreen = s.makeSightGreen;
  w.health = s.health;
  w.lives = s.lives;
  w.kills = s.kills;
  w.timer = s.timer;
  w.killedTimer = s.killedTimer;
  w.currentFrame = s.currentFrame;
  w.flags = s.flags;
  w.ninjarope = s.ninjarope;
  w.currentWeapon = s.currentWeapon;
  w.lastKilledByIdx = s.lastKilledByIdx;
  w.fireCone = s.fireCone;
  w.leaveShellTimer = s.leaveShellTimer;
  std::memcpy(w.reacts, s.reacts, sizeof(w.reacts));
  for (int i = 0; i < NUM_WEAPONS; ++i) w.weapons[i] = s.weapons[i];
  w.direction = s.direction;
  w.controlStates = s.controlStates;
  w.prevControlStates = s.prevControlStates;
  w.steerableSumX = s.steerableSumX;
  w.steerableSumY = s.steerableSumY;
  w.steerableCount = s.steerableCount;
  w.index = s.index;
}

struct GameSnapshot {
  Rand rand;

  int cycles = 0;
  int screenFlash = 0;
  int lastKilledIdx = -1;
  bool gotChanged = false;
  Holdazone holdazone;

  std::array<WormSimState, 2> worms{};

  Game::BonusList bonuses;
  Game::WObjectList wobjects;
  Game::SObjectList sobjects;
  Game::NObjectList nobjects;

  std::vector<BObject> bobjectsArr;
  std::size_t bobjectsCount = 0;

  std::vector<uint8_t> levelData;
  std::vector<Material> levelMaterials;

  uint32_t checksum = 0;

  // Pre-size the dynamic buffers so save/load can avoid allocations.
  // Call once after the level is generated.
  void prepare(Game const& game) {
    bobjectsArr.resize(game.bobjects.limit);
    std::size_t const cells = static_cast<std::size_t>(game.level.width) *
                              static_cast<std::size_t>(game.level.height);
    levelData.resize(cells);
    levelMaterials.resize(cells);
  }
};
