#include <SDL/SDL.h>
#if SDL13
#include <SDL_keysym.h>
#define SDLK_EQUALS '='
#define SDLK_LEFTBRACKET '['
#define SDLK_RIGHTBRACKET ']'
#define SDLK_SEMICOLON ';'
#define SDLK_BACKSLASH '\\'
#define SDLK_COMMA ','
#define SDLK_PERIOD '.'
#define SDLK_SLASH '/'
#endif
#include <cstddef>
#include <cassert>
#include <map>

//int SDLToDOSScanCodes[SDLK_LAST] = {};

std::map<int, int> SDLToDOSScanCodes;

SDLKey const Z = SDLK_UNKNOWN;
SDLKey lieroToSDLKeys[] =
{
	SDLK_UNKNOWN,SDLK_ESCAPE,
	SDLK_1,SDLK_2,SDLK_3,SDLK_4,SDLK_5,SDLK_6,SDLK_7,SDLK_8,SDLK_9,SDLK_0,
	/* 0x0c: */
	SDLK_MINUS,SDLK_EQUALS,SDLK_BACKSPACE,SDLK_TAB,
	SDLK_q,SDLK_w,SDLK_e,SDLK_r,SDLK_t,SDLK_y,SDLK_u,SDLK_i,SDLK_o,SDLK_p,
	SDLK_LEFTBRACKET,SDLK_RIGHTBRACKET,SDLK_RETURN,SDLK_LCTRL,
	SDLK_a,SDLK_s,SDLK_d,SDLK_f,SDLK_g,SDLK_h,SDLK_j,SDLK_k,SDLK_l,
	SDLK_SEMICOLON,SDLK_QUOTE,SDLK_BACKQUOTE,SDLK_LSHIFT,SDLK_BACKSLASH,
	SDLK_z,SDLK_x,SDLK_c,SDLK_v,SDLK_b,SDLK_n,SDLK_m,
	/* 0x33: */
	SDLK_COMMA,SDLK_PERIOD,SDLK_SLASH,SDLK_RSHIFT,SDLK_KP_MULTIPLY,
	SDLK_LALT,SDLK_SPACE,SDLK_CAPSLOCK,
	SDLK_F1,SDLK_F2,SDLK_F3,SDLK_F4,SDLK_F5,SDLK_F6,SDLK_F7,SDLK_F8,SDLK_F9,SDLK_F10,
	/* 0x45: */
	SDLK_NUMLOCK,SDLK_SCROLLOCK,
	SDLK_KP7,SDLK_KP8,SDLK_KP9,SDLK_KP_MINUS,SDLK_KP4,SDLK_KP5,SDLK_KP6,SDLK_KP_PLUS,
	SDLK_KP1,SDLK_KP2,SDLK_KP3,SDLK_KP0,SDLK_KP_PERIOD,
	SDLK_UNKNOWN,SDLK_UNKNOWN,
	SDLK_LESS,SDLK_F11,SDLK_F12,
	/*
	Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,
	Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,
	Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,
	Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z*/
	
	
	Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z,Z, // 27 zeroes
	SDLK_KP_ENTER, // Enter (Pad)
	SDLK_RCTRL, // Right Ctrl
	Z, Z, Z, Z, Z, Z, Z, Z, Z, Z,
	Z, Z, // 12 zeroes
	Z, // Print Screen  TODO: Where is print screen?
	Z, Z, Z, Z, Z, Z, Z, Z, Z, Z, // 10 zeroes
	SDLK_KP_DIVIDE, // / (Pad)
	Z,
	Z, // Print Screen  TODO: Where is print screen?
	SDLK_RALT, // Right Alt
	Z, Z, Z, Z, Z, Z, Z, Z, Z, Z,
	Z, Z, Z, Z, // 14 zeroes
	SDLK_HOME, // Home
	SDLK_UP, // Up
	SDLK_PAGEUP, // Page Up
	Z,
	SDLK_LEFT, // Left
	Z,
	SDLK_RIGHT, // Right
	Z,
	SDLK_END, // End
	SDLK_DOWN, // Down
	SDLK_PAGEDOWN, // Page Down
	SDLK_INSERT, // Insert
	SDLK_DELETE, // Delete
	Z, Z, Z, Z, Z // 5 zeroes
};

Uint32 const maxScanCodes = sizeof(lieroToSDLKeys) / sizeof(*lieroToSDLKeys);

void initKeys()
{
/*
	for(std::size_t i = 0; i < sizeof(SDLToDOSScanCodes) / sizeof(*SDLToDOSScanCodes); ++i)
	{
		SDLToDOSScanCodes[i] = 89;
	}*/
	
	for(std::size_t i = 0; i < maxScanCodes; ++i)
	{
		if(lieroToSDLKeys[i] != SDLK_UNKNOWN)
		{
			SDLToDOSScanCodes[lieroToSDLKeys[i]] = int(i);
		}
	}
}

// Adapted from DOSBOX

#if 0
SDLKey DOSToSDLKey(Uint32 scan)
{
	if(scan < maxScanCodes)
		return lieroToSDLKeys[scan];
	else
		return SDLK_UNKNOWN;
}
#endif

Uint32 SDLToDOSKey(SDLKey key)
{
	std::map<int, int>::iterator i = SDLToDOSScanCodes.find(Uint32(key));
	if(i != SDLToDOSScanCodes.end())
		return i->second;
	return 89;
}

Uint32 SDLToDOSKey(SDL_keysym const& keysym)
{
	Uint32 key = SDLToDOSKey(keysym.sym);
	
	if(key >= 177) // Liero doesn't have keys >= 177
		return 89; // Arbitrarily translate it to 89
	return key;

}
