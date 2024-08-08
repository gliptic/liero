#ifndef UUID_96141CB1E20547016970B28195515A14
#define UUID_96141CB1E20547016970B28195515A14

#include <SDL.h>

//extern int SDLToLieroKeys[SDL_SCANCODE_LAST];
//extern int lieroToSDLKeys[177];

void initKeys();

Uint32 SDLToDOSKey(SDL_Keysym const& keysym);
Uint32 SDLToDOSKey(SDL_Scancode scancode);

int const DkEscape = 1;

int const MaxJoyButtons = 32;

uint32_t const MaxDOSKey 	= 177;
uint32_t const JoyKeysStart	= 512;

inline uint32_t joyButtonToExKey( int joyNum, int joyButton ) {
	return JoyKeysStart + MaxJoyButtons * joyNum + joyButton;
}

inline bool isExtendedKey( uint32_t k ) {
	return k >= MaxDOSKey;
}

const int JoyAxisThreshold = 10000;

#endif // UUID_96141CB1E20547016970B28195515A14
