#pragma once

#include <SDL3/SDL.h>

// extern int SDLToLieroKeys[SDL_SCANCODE_LAST];
// extern int lieroToSDLKeys[177];

void InitKeys();

uint32_t SDLToDOSKey(SDL_Scancode scancode, SDL_Keymod mod);
uint32_t SDLToDOSKey(SDL_Scancode scancode);

int const kDkEscape = 1;

int const kMaxJoyButtons = 32;

uint32_t const kMaxDosKey = 177;
uint32_t const kJoyKeysStart = 512;
// Gamepad control keys start after joystick keys: encode as (player index, control)
uint32_t const kGamepadControlKeysStart = 1024;

inline uint32_t JoyButtonToExKey(int joy_num, int joy_button) {
  return kJoyKeysStart + kMaxJoyButtons * joy_num + joy_button;
}

inline uint32_t GamepadControlToExKey(int player_idx, int control) {
  return kGamepadControlKeysStart + player_idx * 8 + control;
}

inline bool IsGamepadControlKey(uint32_t k) { return k >= kGamepadControlKeysStart; }

inline bool IsExtendedKey(uint32_t k) { return k >= kMaxDosKey; }

const int kJoyAxisThreshold = 10000;
