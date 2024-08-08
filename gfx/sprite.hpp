#ifndef UUID_FB823C685B8D47570B89508CD25CC4A6
#define UUID_FB823C685B8D47570B89508CD25CC4A6

#include <vector>
#include <cstdio>
#include <cassert>
#include "color.hpp"

struct Sprite
{
	PalIdx* mem;
	int width, height, pitch;
};

struct SpriteSet
{
	SpriteSet()
	: width(0), height(0), spriteSize(0), count(0)
	{
	}

	std::vector<PalIdx> data;
	int width;
	int height;
	int spriteSize;
	int count;

	PalIdx* spritePtr(int frame)
	{
		assert(frame >= 0 && frame < count);
		return &data[frame*spriteSize];
	}

	Sprite operator[](int frame)
	{
		assert(frame >= 0 && frame < count);
		Sprite s = {&data[frame*spriteSize], width, height, width};
		return s;
	}

	void allocate(int width, int height, int count);
};

#endif // UUID_FB823C685B8D47570B89508CD25CC4A6
