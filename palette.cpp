#include "palette.hpp"

#include "settings.hpp"
#include "reader.hpp"
#include "gfx.hpp"
#include <SDL/SDL.h>

void Palette::activate()
{
	SDL_Color realPal[256];
	
	for(int i = 0; i < 256; ++i)
	{
		realPal[i].r = entries[i].r << 2;
		realPal[i].g = entries[i].g << 2;
		realPal[i].b = entries[i].b << 2;
	}
	
	SDL_SetColors(gfx.back, realPal, 0, 256);
	SDL_SetColors(gfx.screen, realPal, 0, 256);
}

int fadeValue(int v, int amount)
{
	assert(v < 64);
	v = (v * amount) >> 5;
	if(v < 0) v = 0;
	return v;
}

int lightUpValue(int v, int amount)
{
	v = (v * (32 - amount) + amount*63) >> 5;
	if(v > 63) v = 63;
	return v;
}

void Palette::fade(int amount)
{
	if(amount >= 32)
		return;
		
	for(int i = 0; i < 256; ++i)
	{
		entries[i].r = fadeValue(entries[i].r, amount);
		entries[i].g = fadeValue(entries[i].g, amount);
		entries[i].b = fadeValue(entries[i].b, amount);
	}
}

void Palette::lightUp(int amount)
{
	for(int i = 0; i < 256; ++i)
	{
		entries[i].r = lightUpValue(entries[i].r, amount);
		entries[i].g = lightUpValue(entries[i].g, amount);
		entries[i].b = lightUpValue(entries[i].b, amount);
	}
}

void Palette::rotate(int from, int to)
{
	SDL_Color tocol = entries[to];
	for(int i = to; i > from; --i)
	{
		entries[i] = entries[i - 1];
	}
	entries[from] = tocol;
}

void Palette::clear()
{
	std::memset(entries, 0, sizeof(entries));
}

void Palette::read(FILE* f)
{
	for(int i = 0; i < 256; ++i)
	{
		unsigned char rgb[3];
		fread(rgb, 1, 3, f);
		
		entries[i].r = rgb[0] & 63;
		entries[i].g = rgb[1] & 63;
		entries[i].b = rgb[2] & 63;
	}
}

void Palette::setWormColour(int i, WormSettings const& settings)
{
	int const b[2] = {0x58, 0x78}; // TODO: Read from EXE?
	
	int idx = settings.colour;
	
	setWormColoursSpan(idx, settings.rgb);
	
	for(int j = 0; j < 6; ++j)
	{
		entries[b[i] + j] = entries[idx + (j % 3) - 1];
	}
	
	for(int j = 0; j < 3; ++j)
	{
		entries[129 + i * 4 + j] = entries[idx + j];
	}
}


void Palette::setWormColours(Settings const& settings)
{
	for(int i = 0; i < 2; ++i)
	{
		setWormColour(i, *settings.wormSettings[i]);
	}
}
