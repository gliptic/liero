#include "sprite.hpp"

#include "../reader.hpp"

#include <vector>
#include <cassert>

void SpriteSet::read(ReaderFile& f, int width, int height, int count)
{
	assert(width == height); // We only support rectangular sprites right now
	
	this->width = width;
	this->height = height;
	this->spriteSize = width * height;
	this->count = count;
	
	int amount = spriteSize * count;
	data.resize(amount);
	
	std::vector<uint8_t> temp(amount);
	
	f.get(&temp[0], amount);
	
	PalIdx* dest = &data[0];
	uint8_t* src = &temp[0];
	
	for(int i = 0; i < count; i++)
	{
		for(int x = 0; x < width; ++x)
		{
			for(int y = 0; y < height; ++y)
			{
				dest[x + y*width] = src[y];
			}
			
			src += height;
		}
		
		dest += spriteSize;
	}
}

void SpriteSet::allocate(int width, int height, int count)
{
	this->width = width;
	this->height = height;
	this->spriteSize = width * height;
	this->count = count;
	
	int amount = spriteSize * count;
	data.resize(amount);
}
