#ifndef LIERO_GFX_BITMAP_HPP
#define LIERO_GFX_BITMAP_HPP

struct Bitmap
{
	int w, h;
	unsigned int pitch;
	unsigned char* pixels;
	SDL_Rect clip_rect;

	unsigned char& getPixel(int x, int y)
	{
		return (static_cast<unsigned char*>(pixels) + y*pitch)[x];
	}
};

#endif // LIERO_GFX_BITMAP_HPP
