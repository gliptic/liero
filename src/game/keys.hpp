#pragma once

#include <SDL3/SDL.h>

//extern int SDLToLieroKeys[SDL_SCANCODE_LAST];
//extern int lieroToSDLKeys[177];

void initKeys();

uint32_t SDLToDOSKey(SDL_Scancode scancode, SDL_Keymod mod);
uint32_t SDLToDOSKey(SDL_Scancode scancode);

int const DkEscape = 1;

int const MaxJoyButtons = 32;

uint32_t const MaxDOSKey 	= 177;
uint32_t const JoyKeysStart	= 512;
// Gamepad control keys start after joystick keys: encode as (gamepad index, control)
uint32_t const GamepadControlKeysStart = 1024;

inline uint32_t joyButtonToExKey( int joyNum, int joyButton ) {
	return JoyKeysStart + MaxJoyButtons * joyNum + joyButton;
}

inline uint32_t gamepadControlToExKey( int gpIdx, int control ) {
	return GamepadControlKeysStart + gpIdx * 8 + control;
}

inline bool isGamepadControlKey( uint32_t k ) {
	return k >= GamepadControlKeysStart;
}

inline bool isExtendedKey( uint32_t k ) {
	return k >= MaxDOSKey;
}

const int JoyAxisThreshold = 10000;
