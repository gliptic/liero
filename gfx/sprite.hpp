#ifndef UUID_FB823C685B8D47570B89508CD25CC4A6
#define UUID_FB823C685B8D47570B89508CD25CC4A6

#include <vector>
#include <cstdio>
#include "color.hpp"

struct ReaderFile;

struct Sprite
{
	PalIdx* mem;
	int width, height, pitch;
};

struct SpriteSet
{
	std::vector<PalIdx> data;
	int width;
	int height;
	int spriteSize;
	int count;
	
	void read(ReaderFile& f, int width, int height, int count);
	
	PalIdx* spritePtr(int frame)
	{
		return &data[frame*spriteSize];
	}

	Sprite operator[](int frame)
	{
		Sprite s = {&data[frame*spriteSize], width, height, width};
		return s;
	}
	
	void allocate(int width, int height, int count);
};

#endif // UUID_FB823C685B8D47570B89508CD25CC4A6
