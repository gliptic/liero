#ifndef UUID_6327FB2089BF4EFC31D42D9AA180316B
#define UUID_6327FB2089BF4EFC31D42D9AA180316B

#include "config.h"
#include "stream.h"
#include "image.h"

typedef struct tl_png {
	tl_byte_source_pullable in;
	tl_image img;
} tl_png;

#define PNGERR_BAD_SIG (-1)
#define PNGERR_BAD_IHDR (-2)
#define PNGERR_UNSUPPORTED_FORMAT (-3)
#define PNGERR_IHDR_NOT_FOUND (-4)
#define PNGERR_INVALID_PALETTE (-5)
#define PNGERR_INVALID_TRANSPARENCY (-6)
#define PNGERR_INFLATE_ERROR (-7)
#define PNGERR_INVALID_PARAMETER (-8)
#define PNGERR_IDAT_NOT_FOUND (-9)
#define PNGERR_INVALID_IDAT (-10)

TL_INF_API tl_png* tl_png_create(void);
TL_INF_API tl_png* tl_png_create_file(char const* path);
TL_INF_API int tl_png_load(tl_png* self, uint32 req_comp);
TL_INF_API void tl_png_destroy(tl_png* self);

TL_INLINE uint8* tl_png_release_img(tl_png* self)
{
	uint8* img = self->img.pixels;
	self->img.pixels = NULL;
	return img;
}

#endif // UUID_6327FB2089BF4EFC31D42D9AA180316B
