#include "font.hpp"
#include "../reader.hpp"
#include "../gfx.hpp"
#include "macros.hpp"
#include "color.hpp"

void Font::drawChar(Bitmap& scr, unsigned char c, int x, int y, int color, int size)
{
	if(c >= 2 && c < 252) // TODO: Is this correct, shouldn't it be c >= 0 && c < 250, since drawText subtracts 2?
	{
		uint8_t* mem = chars[c].data;
		int width = 7;
		int height = 8;
		int pitch = 7;

		CLIP_IMAGE(scr.clip_rect);

		PalIdx* scrptr = static_cast<PalIdx*>(scr.pixels) + y*scr.pitch + x;

		for(int cy = 0; cy < height; ++cy)
		{
			for(int i = 0; i < size; i++)
			{
				PalIdx* rowdest = scrptr;
				PalIdx* rowsrc = mem;

				for(int cx = 0; cx < width; ++cx)
				{
					PalIdx c = *rowsrc;
					for(int k = 0; k < size; k++)
					{
						if(c)
						{
							*rowdest = color;
						}
						++rowdest;
					}
					++rowsrc;
				}

				scrptr += scr.pitch;
			}
			mem += pitch;
		}
	}
}

void Font::drawText(Bitmap& scr, char const* str, std::size_t len, int x, int y, int color, int size)
{
	int orgX = x;

	for(std::size_t i = 0; i < len; ++str, ++i)
	{
		unsigned char c = static_cast<unsigned char>(*str);

		if(!c)
		{
			x = orgX;
			y += 8 * size;
		}
		else if(c >= 2 && c < 252)
		{
			c -= 2;

			drawChar(scr, c, x, y, color, size);

			x += chars[c].width * size;
		}
	}
}

void Font::drawFramedText(Bitmap& scr, std::string const& text, int x, int y, int color)
{
	drawRoundedBox(scr, x, y, 0, 7, getDims(text));
	drawText(scr, text, x + 2, y + 1, color);
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
