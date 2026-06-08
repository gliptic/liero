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
  fixedvec pos, vel;
  IVec2 logic_respawn;
  int hotspot_x = 0, hotspot_y = 0;
  fixed aiming_angle = 0, aiming_speed = 0;
  bool able_to_jump = false, able_to_dig = false;
  bool key_change_pressed = false;
  bool movable = false;
  bool animate = false, visible = false, ready = false, flag = false;
  bool make_sight_green = false;
  int health = 0, lives = 0, kills = 0;
  int timer = 0, killed_timer = 0;
  int current_frame = 0;
  int flags = 0;
  Ninjarope ninjarope;
  int current_weapon = 0;
  int last_killed_by_idx = -1;
  int fire_cone = 0;
  int leave_shell_timer = 0;
  int reacts[4] = {0, 0, 0, 0};
  WormWeapon weapons[NUM_WEAPONS];
  int direction = 0;
  Worm::ControlState control_states, prev_control_states;
  int steerable_sum_x = 0, steerable_sum_y = 0, steerable_count = 0;
  int index = 0;
};

inline void SaveWormSimState(WormSimState& s, Worm const& w) {
  s.pos = w.pos;
  s.vel = w.vel;
  s.logic_respawn = w.logic_respawn;
  s.hotspot_x = w.hotspot_x;
  s.hotspot_y = w.hotspot_y;
  s.aiming_angle = w.aiming_angle;
  s.aiming_speed = w.aiming_speed;
  s.able_to_jump = w.able_to_jump;
  s.able_to_dig = w.able_to_dig;
  s.key_change_pressed = w.key_change_pressed;
  s.movable = w.movable;
  s.animate = w.animate;
  s.visible = w.visible;
  s.ready = w.ready;
  s.flag = w.flag;
  s.make_sight_green = w.make_sight_green;
  s.health = w.health;
  s.lives = w.lives;
  s.kills = w.kills;
  s.timer = w.timer;
  s.killed_timer = w.killed_timer;
  s.current_frame = w.current_frame;
  s.flags = w.flags;
  s.ninjarope = w.ninjarope;
  s.current_weapon = w.current_weapon;
  s.last_killed_by_idx = w.last_killed_by_idx;
  s.fire_cone = w.fire_cone;
  s.leave_shell_timer = w.leave_shell_timer;
  std::memcpy(s.reacts, w.reacts, sizeof(s.reacts));
  for (int i = 0; i < NUM_WEAPONS; ++i) s.weapons[i] = w.weapons[i];
  s.direction = w.direction;
  s.control_states = w.control_states;
  s.prev_control_states = w.prev_control_states;
  s.steerable_sum_x = w.steerable_sum_x;
  s.steerable_sum_y = w.steerable_sum_y;
  s.steerable_count = w.steerable_count;
  s.index = w.index;
}

inline void RestoreWormSimState(Worm& w, WormSimState const& s) {
  w.pos = s.pos;
  w.vel = s.vel;
  w.logic_respawn = s.logic_respawn;
  w.hotspot_x = s.hotspot_x;
  w.hotspot_y = s.hotspot_y;
  w.aiming_angle = s.aiming_angle;
  w.aiming_speed = s.aiming_speed;
  w.able_to_jump = s.able_to_jump;
  w.able_to_dig = s.able_to_dig;
  w.key_change_pressed = s.key_change_pressed;
  w.movable = s.movable;
  w.animate = s.animate;
  w.visible = s.visible;
  w.ready = s.ready;
  w.flag = s.flag;
  w.make_sight_green = s.make_sight_green;
  w.health = s.health;
  w.lives = s.lives;
  w.kills = s.kills;
  w.timer = s.timer;
  w.killed_timer = s.killed_timer;
  w.current_frame = s.current_frame;
  w.flags = s.flags;
  w.ninjarope = s.ninjarope;
  w.current_weapon = s.current_weapon;
  w.last_killed_by_idx = s.last_killed_by_idx;
  w.fire_cone = s.fire_cone;
  w.leave_shell_timer = s.leave_shell_timer;
  std::memcpy(w.reacts, s.reacts, sizeof(w.reacts));
  for (int i = 0; i < NUM_WEAPONS; ++i) w.weapons[i] = s.weapons[i];
  w.direction = s.direction;
  w.control_states = s.control_states;
  w.prev_control_states = s.prev_control_states;
  w.steerable_sum_x = s.steerable_sum_x;
  w.steerable_sum_y = s.steerable_sum_y;
  w.steerable_count = s.steerable_count;
  w.index = s.index;
}

struct GameSnapshot {
  Rand rand;

  int cycles = 0;
  int screen_flash = 0;
  int last_killed_idx = -1;
  bool got_changed = false;
  Holdazone holdazone;

  std::array<WormSimState, 2> worms{};

  Game::BonusList bonuses;
  Game::WObjectList wobjects;
  Game::SObjectList sobjects;
  Game::NObjectList nobjects;

  std::vector<BObject> bobjects_arr;
  std::size_t bobjects_count = 0;

  std::vector<uint8_t> level_data;
  std::vector<Material> level_materials;

  uint32_t checksum = 0;

  // Pre-size the dynamic buffers so save/load can avoid allocations.
  // Call once after the level is generated.
  void Prepare(Game const& game) {
    bobjects_arr.resize(game.bobjects.limit);
    std::size_t const kCells =
        static_cast<std::size_t>(game.level.width) * static_cast<std::size_t>(game.level.height);
    level_data.resize(kCells);
    level_materials.resize(kCells);
  }
};
