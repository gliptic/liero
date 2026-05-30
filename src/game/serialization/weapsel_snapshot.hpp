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
  bool wsDone = false;  // true if WeaponSelection::processFrame returned
                        // true at this frame (both peers pressed Done).

  struct PerPlayer {
    std::array<uint32_t, Settings::selectableWeapons> weapons{};
    bool isReady = false;
    int menuSelection = 0;
    int menuTopItem = 0;
    int menuBottomItem = 0;
    uint16_t wormControlStates = 0;
    int currentWeapon = 0;
  };
  std::array<PerPlayer, 2> players{};

  Rand rand;

  // Edge-detection / key-repeat state on the rollback controller.
  uint8_t localPrevInput = 0;
  uint8_t remotePrevInput = 0;
  std::array<uint16_t, 8> localHeldFrames{};
  std::array<uint16_t, 8> remoteHeldFrames{};
};
