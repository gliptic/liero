#ifndef UUID_412A1D6C55034518F1C26BB9CA854B2C
#define UUID_412A1D6C55034518F1C26BB9CA854B2C

#include "config.h"
#include "rect.h"

typedef struct tl_packed_rect
{
	tl_recti rect;
} tl_packed_rect;

typedef struct tl_rectpack
{
	tl_packed_rect base;
	struct tl_rectpack* parent;
	int largest_free_width, largest_free_height;
	tl_recti enclosing;
	struct tl_rectpack* ch[2];
	char occupied;
} tl_rectpack;

TL_RECT_API void            tl_rectpack_init(tl_rectpack* self, tl_recti r);
TL_RECT_API tl_packed_rect* tl_rectpack_try_fit(tl_rectpack* self, int w, int h, int allow_rotate);
TL_RECT_API void            tl_rectpack_remove(tl_rectpack* self, tl_packed_rect* r);
TL_RECT_API void            tl_rectpack_deinit(tl_rectpack* self);

#endif // UUID_412A1D6C55034518F1C26BB9CA854B2C
