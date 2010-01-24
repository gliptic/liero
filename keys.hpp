#ifndef UUID_96141CB1E20547016970B28195515A14
#define UUID_96141CB1E20547016970B28195515A14

#include <SDL/SDL.h>

//extern int SDLToLieroKeys[SDLK_LAST];
//extern int lieroToSDLKeys[177];

void initKeys();

Uint32 SDLToDOSKey(SDL_keysym const& keysym);
Uint32 SDLToDOSKey(SDLKey key);
SDLKey DOSToSDLKey(Uint32 scan);

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
