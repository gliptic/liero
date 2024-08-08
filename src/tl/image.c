#include "image.h"

#include <string.h>
#include <assert.h>
#include <malloc.h>

#define tl_image_ptr(img, x, y, bpp) ((img)->pixels + (y)*(img)->pitch + (x)*(bpp))
#define tl_image_pitch(img) ((img)->pitch)
#define tl_image_size(img) ((img)->pitch * (img)->h)

void tl_blit_unsafe(tl_image* to, tl_image* from, int x, int y)
{
	uint8* tp = tl_image_ptr(to, x, y, to->bpp);
	uint8* fp = from->pixels;
	uint32 tpitch = tl_image_pitch(to);
	uint32 fline = from->w * from->bpp;
	uint32 fpitch = tl_image_pitch(from);
	uint32 hleft = from->h;

	while(hleft-- > 0)
	{
		memcpy(tp, fp, fline);
		tp += tpitch;
		fp += fpitch;
	}
}

int tl_image_convert(tl_image* to, tl_image* from)
{
	uint32 hleft = from->h;
	uint32 w = from->w;
	int fbpp = from->bpp;
	int tbpp = to->bpp;
	uint8* tp = to->pixels;
	uint8* fp = from->pixels;
	uint32 tpitch = tl_image_pitch(to);
	uint32 fpitch = tl_image_pitch(from);
	uint32 i;

	if(to->w != from->w || to->h != from->h)
		return -1;

	assert(fbpp >= 1 && fbpp <= 4);
	assert(tbpp >= 1 && tbpp <= 4);

	{
		uint32 id = ((fbpp << 2) + tbpp) - ((1<<2)+1);
		// id is a value in [0, 16)

		#define FT(f,t) ((((f)-1)<<2)+((t)-1))

		if(fbpp == tbpp)
		{
			uint32 fline = from->w * from->bpp;
			for(; hleft-- > 0; tp += tpitch, fp += fpitch)
				memcpy(tp, fp, fline);
		}
		else switch (id)
		{
			case FT(1,4):
			{
				for(; hleft-- > 0; tp += tpitch, fp += fpitch)
				for(i = 0; i < w; ++i)
				{
					uint8 f = fp[i];
					/*
					tp[i*4  ] = f;
					tp[i*4+1] = f;
					tp[i*4+2] = f;
					tp[i*4+3] = 255;*/
					tp[i*4  ] = 255;
					tp[i*4+1] = 255;
					tp[i*4+2] = 255;
					tp[i*4+3] = f;
				}
				break;
			}
		}

		#undef FT
	}

	return 0;

	// 1 L
	// 2 L A
	// 3 R G B
	// 4 R G B A


	// L       -> L *
	// L       -> L L L
	// L       -> L L L *
	// L A     -> L
	// L A     -> L L L
	// L A     -> L L L A
	// R G B   -> M
	// R G B   -> M *
	// R G B   -> R G B *
	// R G B A -> M
	// R G B A -> M *
	// R G B A -> R G B
}

int tl_image_pad(tl_image* to, tl_image* from)
{
	assert(to->w == from->w + 2);
	assert(to->h == from->h + 2);

	memset(to->pixels, 0, tl_image_size(to));
	tl_blit_unsafe(to, from, 1, 1);
	return 0;
}

void tl_image_init(tl_image* self, uint32 w, uint32 h, int bpp)
{
	self->pixels = malloc(w*h*bpp);
	self->w = w;
	self->h = h;
	self->pitch = w * bpp;
	self->bpp = bpp;
}