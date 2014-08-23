#ifndef TL_REGION_H
#define TL_REGION_H

#include <stddef.h>
#include <stdlib.h>

#include "platform.h"
#include "config.h"

typedef struct tl_region_block {
	struct tl_region_block* next;
	char mem[1];
} tl_region_block;

typedef struct tl_region {
	tl_region_block* first_block;
	char* cur;
	char* end;
} tl_region;

#define TL_REGION_MAX_ALIGN (8)

TL_REGION_API void* tl_region_alloc_newblock(tl_region* r, size_t rounded_size);
TL_REGION_API void tl_region_free(tl_region* r);

TL_INLINE void* tl_region_alloc(tl_region* r, size_t size) {
	size_t rounded_size = (size + (TL_REGION_MAX_ALIGN-1)) & ~((size_t)TL_REGION_MAX_ALIGN - 1);

	if((size_t)(r->end - r->cur) >= rounded_size) {
		char* ret = r->cur;
		r->cur += rounded_size;
		return ret;
	} else {
		return tl_region_alloc_newblock(r, rounded_size);
	}
}

#define tl_region_init(r) do { \
	tl_region* _r = (r); \
	_r->first_block = NULL; \
	_r->end = _r->cur = NULL; \
} while(0)


#endif // TL_REGION_H

