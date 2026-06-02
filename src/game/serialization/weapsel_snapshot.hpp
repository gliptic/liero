#pragma once

// Rollback snapshot for the weapon-select phase.
//
// Captures the mutable state WeaponSelection touches each frame so the
// rollback controller can predict + resim weapon-select frames the same
// way it does game frames: menus, isReady flags, picked weapon IDs
// (= worm.settings->weapons[]), worm.controlStates, the controller's
// edge-detection / key-repeat state, and game.rand (used by Randomize).
//
// Menu item display strings and worm.weapons[].type pointers are NOT
// stored — both are derivable from the weapon IDs on restore via
// Common::weapOrder / Common::weapons.

#include "../rand.hpp"
#include "../settings.hpp"

#include <array>
#include <cstdint>

struct WeaponSelectSnap {
  bool valid = false;
  bool ws_done = false;  // true if WeaponSelection::processFrame returned
                         // true at this frame (both peers pressed Done).

  struct PerPlayer {
    std::array<uint32_t, Settings::kSelectableWeapons> weapons{};
    bool is_ready = false;
    int menu_selection = 0;
    int menu_top_item = 0;
    int menu_bottom_item = 0;
    uint16_t worm_control_states = 0;
    int current_weapon = 0;
  };
  std::array<PerPlayer, 2> players{};

  Rand rand;

  // Edge-detection / key-repeat state on the rollback controller.
  uint8_t local_prev_input = 0;
  uint8_t remote_prev_input = 0;
  std::array<uint16_t, 8> local_held_frames{};
  std::array<uint16_t, 8> remote_held_frames{};
};
