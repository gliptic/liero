#ifndef UUID_74C8EE76D5564F2D8C7BBC9B26C16192
#define UUID_74C8EE76D5564F2D8C7BBC9B26C16192

#include <SDL/SDL.h>
#include <cstdio>
#include <gvl/support/debug.hpp>

struct Settings;

struct Palette
{
	SDL_Color entries[256];
	
	// TODO: Move definitions of these from gfx.cpp to palette.cpp
	void activate();
	void fade(int amount);
	void lightUp(int amount);
	void rotate(int from, int to);
	void read(FILE* f);
	
	void scaleAdd(int dest, int const(&c)[3], int scale, int add)
	{
		entries[dest].r = (add + c[0] * scale) / 64;
		entries[dest].g = (add + c[1] * scale) / 64;
		entries[dest].b = (add + c[2] * scale) / 64;
		
		sassert(entries[dest].r < 64);
		sassert(entries[dest].g < 64);
		sassert(entries[dest].b < 64);
	}
		
	void setWormColoursSpan(int base, int const (&c)[3])
	{
		scaleAdd(base - 2, c, 38, 0);
		scaleAdd(base - 1, c, 50, 0);
		scaleAdd(base    , c, 64, 0);
		scaleAdd(base + 1, c, 47, 1008);
		scaleAdd(base + 2, c, 28, 2205);
	}
	
	void resetPalette(Palette const& newPal, Settings const& settings)
	{
		*this = newPal;
		setWormColours(settings);
	}
	
	void setWormColours(Settings const& settings);
	
	void clear();
};

#endif // UUID_74C8EE76D5564F2D8C7BBC9B26C16192
