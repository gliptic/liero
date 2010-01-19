#include "font.hpp"
#include "../reader.hpp"
#include "../gfx.hpp"
#include "macros.hpp"
#include "color.hpp"
#include <iostream>

void Font::loadFromEXE()
{
	chars.resize(250);
	
	std::size_t const FontSize = 250 * 8 * 8 + 1;
	std::vector<unsigned char> temp(FontSize);
	
	FILE* exe = openLieroEXE();
	
	fseek(exe, 0x1C825, SEEK_SET);
	
	checkedFread(&temp[0], 1, FontSize, exe);
	
	for(int i = 0; i < 250; ++i)
	{
		unsigned char* ptr = &temp[i*64 + 1];
		
		for(int y = 0; y < 8; ++y)
		{
			for(int x = 0; x < 7; ++x)
			{
				chars[i].data[y*7 + x] = ptr[y*8 + x];
			}
		}
		
		chars[i].width = ptr[63];
	}
}

void Font::drawChar(unsigned char c, int x, int y, int color)
{
	if(c >= 2 && c < 252) // TODO: Is this correct, shouldn't it be c >= 0 && c < 250, since drawText subtracts 2?
	{
		SDL_Surface* scr = gfx.screen;
		uint8_t* mem = chars[c].data;
		int width = 7;
		int height = 8;
		int pitch = 7;
		
		CLIP_IMAGE(scr->clip_rect);
		
		PalIdx* scrptr = static_cast<PalIdx*>(scr->pixels) + y*scr->pitch + x;
		
		for(int cy = 0; cy < height; ++cy)
		{
			PalIdx* rowdest = scrptr;
			PalIdx* rowsrc = mem;
			
			for(int cx = 0; cx < width; ++cx)
			{
				PalIdx c = *rowsrc;
				if(c)
					*rowdest = color;
				++rowsrc;
				++rowdest;
			}

			scrptr += scr->pitch;
			mem += pitch;
		}
	}
}

void Font::drawText(char const* str, std::size_t len, int x, int y, int color)
{
	int orgX = x;
	
	for(std::size_t i = 0; i < len; ++str, ++i)
	{
		unsigned char c = static_cast<unsigned char>(*str);
		
		if(!c)
		{
			x = orgX;
			y += 8;
		}
		else if(c >= 2 && c < 252)
		{
			c -= 2;
			
			drawChar(c, x, y, color);
			
			x += chars[c].width;
		}
	}
}

void Font::drawFramedText(std::string const& text, int x, int y, int color)
{
	drawRoundedBox(x, y, 0, 7, getDims(text));
	drawText(text, x + 2, y + 1, color);
}

int Font::getDims(char const* str, std::size_t len, int* height)
{
	int width = 0;
	int maxHeight = 8;
	
	int maxWidth = 0;
	
	for(std::size_t i = 0; i < len; ++str, ++i)
	{
		unsigned char c = static_cast<unsigned char>(*str);
		if(c >= 2 && c < 252)
			width += chars[c - 2].width;
		else if(!c)
		{
			maxWidth = std::max(maxWidth, width);
			width = 0;
			maxHeight += 8;
		}
	}
	
	if(height)
		*height = maxHeight;
	
	return std::max(maxWidth, width);
}
