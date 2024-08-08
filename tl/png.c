#include "png.h"

#include <stdio.h> // TEMP

#include "coro.h"
#include "inflate_impl.h"

typedef struct tl_png_source {
	tl_png base;

	tl_inflate_source inf;
	uint32 img_n;
} tl_png_source;

void tl_png_init_(tl_png_source* self)
{
	tl_inf_init_(&self->inf);
	tl_bs_init_source(&self->base.in);
	self->base.img.pixels = NULL;
}

#define get8(self) tl_bs_pull_def0(&self->base.in)
#define get16(self) tl_bs_pull16_def0(&self->base.in)
#define get32(self) tl_bs_pull32_def0(&self->base.in)
#define skip(self, n) tl_bs_pull_skip(&self->base.in, (n))

static uint8 png_sig[8] = { 137,80,78,71,13,10,26,10 };


#define PNG_TYPE(a,b,c,d)  (((a) << 24) + ((b) << 16) + ((c) << 8) + (d))

#define CHECK(f)  do { int r_ = (f); if (r_) return r_; } while(0)

tl_png* tl_png_create(void)
{
	tl_png_source* self = malloc(sizeof(tl_png_source));
	tl_png_init_(self);
	return (tl_png*)self;
}

void tl_png_destroy(tl_png* self)
{
	// TODO: tl_inf_deinit_ in case inflate ever needs to do something
	tl_bs_free(&self->in);
	free(self->img.pixels);
	free(self);
}

enum {
	F_none=0, F_sub=1, F_up=2, F_avg=3, F_paeth=4,
	F_avg_first, F_paeth_first
};

static uint8 first_row_filter[5] =
{
	F_none, F_sub, F_none, F_avg_first, F_paeth_first

	// res = ((x>>1)&2) | (x&1) | ((x+1)&4)
};

static int paeth(int a, int b, int c)
{
	int p = a + b - c;
	int pa = abs(p-a);
	int pb = abs(p-b);
	int pc = abs(p-c);
	if (pa <= pb && pa <= pc) return a;
	if (pb <= pc) return b;
	return c;
}

int tl_png_load(tl_png* self_, uint32 req_comp)
{
	uint32 i, pal_img_n = 0;
	tl_png_source* self = (tl_png_source*)self_;
	int seen_ihdr = 0, has_trans = 0;
	int interlace;
	uint32 pal_len = 0;
	uint8* img = NULL;
	uint8* out = NULL;
	uint8* scanline = NULL;
	uint32 total_size;
	uint32 scanline_size;
	uint8 palette[1024], tc[3];

	if(req_comp > 4)
		return PNGERR_INVALID_PARAMETER;

	// Check header
	for(i = 0; i < 8; ++i)
		if(get8(self) != png_sig[i]) return PNGERR_BAD_SIG;

	while(1)
	{
		uint32 length = get32(self);
		uint32 type = get32(self);

		switch(type)
		{
			case PNG_TYPE('I','H','D','R'):
			{
				int depth,color,comp,filter;

				self->base.img.w = get32(self);
				self->base.img.h = get32(self);
				depth = get8(self);
				color = get8(self);
				if(depth != 8 || self->base.img.w > (1<<24) || self->base.img.h > (1<<24)) return PNGERR_UNSUPPORTED_FORMAT;

				comp = get8(self);
				filter = get8(self);
				interlace = get8(self);

				if(seen_ihdr || length != 13
				|| color > 6 || comp != 0
				|| filter != 0 || interlace > 0 /* TODO: interlace not supported yet */
				|| self->base.img.w == 0 || self->base.img.h == 0) return PNGERR_BAD_IHDR;

				if(color == 3)
					pal_img_n = 3;
				else if(color & 1)
					return PNGERR_BAD_IHDR;

				seen_ihdr = 1;

				if(!pal_img_n)
				{
					self->img_n = (color & 2 ? 3 : 1) + (color & 4 ? 1 : 0);
					if((1<<30) / self->base.img.w / self->img_n < self->base.img.h) return PNGERR_UNSUPPORTED_FORMAT;

					// TODO: Quit if only reading header
				}
				else
				{
					// if paletted, then pal_n is our final components, and
					// img_n is # components to decompress/filter.
					self->img_n = 1;
					if((1<<30) / self->base.img.w / 4 < self->base.img.h) return PNGERR_UNSUPPORTED_FORMAT;
					// if SCAN_header, have to scan to see if we have a tRNS
				}
				break;
			}

			case PNG_TYPE('P','L','T','E'):
			{
				if(!seen_ihdr) return PNGERR_IHDR_NOT_FOUND;
				if(length > 256*3) return PNGERR_INVALID_PALETTE;
				pal_len = length / 3;
				if(pal_len * 3 != length) return PNGERR_INVALID_PALETTE;
				for(i = 0; i < pal_len; ++i)
				{
				   palette[i*4+0] = get8(self);
				   palette[i*4+1] = get8(self);
				   palette[i*4+2] = get8(self);
				   palette[i*4+3] = 255;
				}
				break;
			}

			case PNG_TYPE('t','R','N','S'):
			{
				if(!seen_ihdr) return PNGERR_IHDR_NOT_FOUND;
				// TODO: if (z->idata) return e("tRNS after IDAT","Corrupt PNG");
				if (pal_img_n)
				{
					// TODO: if (scan == SCAN_header) { s->img_n = 4; return 1; }
					if(pal_len == 0) return PNGERR_INVALID_PALETTE;
					if(length > pal_len) return PNGERR_INVALID_TRANSPARENCY;
					pal_img_n = 4;
					for(i = 0; i < length; ++i)
						palette[i*4+3] = get8(self);
				}
				else
				{
					if(!(self->img_n & 1)) return PNGERR_INVALID_TRANSPARENCY;
					if(length != (uint32)self->img_n*2) return PNGERR_INVALID_TRANSPARENCY;
					has_trans = 1;
					for(i = 0; i < self->img_n; ++i)
						tc[i] = (uint8) get16(self); // non 8-bit images will be larger
				}
				break;
			}

			case PNG_TYPE('I','D','A','T'):
			{
				int ret = ZERR_UNDERFLOW;
				if(!seen_ihdr) return PNGERR_IHDR_NOT_FOUND;
				if(pal_img_n && !pal_len) return PNGERR_INVALID_PALETTE;
				// TODO: if (scan == SCAN_header) { s->img_n = pal_img_n; return 1; }

				if(NULL == img)
				{
					if(pal_img_n)
					{
						self->base.img.bpp = pal_img_n;
						if (req_comp >= 3)
							self->base.img.bpp = req_comp;
					}
					else if((req_comp == self->img_n+1 && req_comp != 3) || has_trans)
						self->base.img.bpp = self->img_n + 1;
					else
						self->base.img.bpp = self->img_n;

					total_size = self->base.img.w * self->base.img.h * self->base.img.bpp;
					img = malloc(total_size);
					free(self->base.img.pixels);
					self->base.img.pixels = img; // Save immediately so that it isn't leaked
					self->base.img.pitch = self->base.img.w * self->base.img.bpp;
					out = img;

					scanline_size = self->base.img.w * self->img_n;
					scanline = malloc(1 + scanline_size);

					if(self->base.img.bpp > self->img_n)
						memset(img, 255, total_size); // Need to add alphachannel, so fill it all with 0xff

					self->inf.base.out.out = scanline;
					self->inf.base.out.out_start = scanline;
					self->inf.base.out.out_end = scanline + 1 + scanline_size;
				}

				// TODO: Make sure we only write self->base.h rows

				while(ret == ZERR_UNDERFLOW)
				{
					// TODO: Utility function for this?

					// TODO: If length == 0 here after a tl_inf_run, there is something wrong

					CHECK(tl_bs_check_pull(&self->base.in));

					self->inf.base.in.buf = self->base.in.buf;
					self->inf.base.in.buf_end = self->base.in.buf_end;
					if(tl_bs_left(&self->inf.base.in) >= length) // >= to always do flush
					{
						self->inf.base.in.buf_end = self->inf.base.in.buf + length;
						self->inf.base.flush = 1;
					}
					self->base.in.buf = self->inf.base.in.buf_end;
					length -= tl_bs_left(&self->inf.base.in);

					do
					{
						ret = tl_inf_run(&self->inf.base);

						if(self->inf.base.out.out == self->inf.base.out.out_end)
						{
							// We have a full scanline
							uint8* p = scanline;
							uint8 left[4] = {0}, above[4] = {0};
							uint8 filter = *p++;
							uint32 k;
							uint32 img_n = self->img_n;
							uint32 bpp = self->base.img.bpp;
							int32 npitch = -(int32)self->base.img.w * bpp;

							if(filter > F_paeth)
								return PNGERR_INVALID_IDAT;
							if(img == out)
								filter = first_row_filter[filter];

							{
								#define CASE(f) \
									case f:     \
										for (i = self->base.img.w; i-- > 0; out += bpp) \
											for (k = 0; k < img_n; ++k)
								switch (filter) {
									CASE(F_none)        { out [k] = *p++; } break;
									CASE(F_sub)         { left[k] += *p++; out[k] = left[k]; } break;
									CASE(F_up)          { out [k] = *p++ + out[k+npitch]; } break;
									CASE(F_avg)         { left[k] = *p++ + ((out[k+npitch] + left[k]) >> 1); out[k] = left[k]; } break;
									CASE(F_paeth)       {
										left[k] = (uint8)(*p++ + paeth(left[k], out[k+npitch], above[k]));
										above[k] = out[k+npitch];
										out[k] = left[k];
									} break;
									CASE(F_avg_first)   { left[k] = *p++ + (left[k] >> 1); out[k] = left[k]; } break;
									CASE(F_paeth_first) { left[k] = (uint8)(*p++ + paeth(left[k], 0, 0)); out[k] = left[k]; } break;
								}
								#undef CASE
							}

							if (has_trans)
							{
								// Expand color-based transparency
								out += npitch;
								for (i = self->base.img.w; i-- > 0; out += bpp)
								{
									for (k = 0; k < img_n; ++k)
										if(out[k] != tc[k]) goto not_trans;

									out[img_n] = 0;
								not_trans:;
								}
							}

							self->inf.base.out.out = scanline;
						}
					}
					while(ret == ZERR_OVERFLOW);
				}

				if(length > 0)
					skip(self, length);

				if(ret != ZERR_OK)
					return PNGERR_INFLATE_ERROR;

				break;
			}

			case PNG_TYPE('I','E','N','D'):
			{
				// IDAT checks for IHDR, so it's enough to check for IDAT
				// TODO: if (scan != SCAN_load) return 1;
				if (NULL == img) return PNGERR_IDAT_NOT_FOUND;

				if (pal_img_n && self->base.img.bpp != 1)
				{
					uint8* p = img;
					uint32 bpp = self->base.img.bpp;
					// Expand palette to 3 or 4 components
					assert(bpp == 3 || bpp == 4);

					for (i = self->base.img.w * self->base.img.h; i-- > 0; p += bpp)
					{
						uint8 idx = p[0];

						p[0] = palette[idx*4 + 0];
						p[1] = palette[idx*4 + 1];
						p[2] = palette[idx*4 + 2];
						if(bpp == 4)
							p[3] = palette[idx*4 + 3];
					}
				}

				return 1;
			}

			default:
			{
				skip(self, length);
				break;
			}
		}

		// TODO: Check whether the stream is depleted. We should error then.

		(void)get32(self);
	}
}