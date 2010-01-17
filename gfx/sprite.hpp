#ifndef UUID_FB823C685B8D47570B89508CD25CC4A6
#define UUID_FB823C685B8D47570B89508CD25CC4A6

#include <vector>
#include <cstdio>
#include "colour.hpp"

struct SpriteSet
{
	std::vector<PalIdx> data;
	int width;
	int height;
	int spriteSize;
	int count;
	
	void read(FILE* f, int width, int height, int count);
	
	PalIdx* spritePtr(int frame)
	{
		return &data[frame*spriteSize];
	}
	
	void allocate(int width, int height, int count);
};

#endif // UUID_FB823C685B8D47570B89508CD25CC4A6
