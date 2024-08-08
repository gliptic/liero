#include "region.h"

#define ALIGN_PTR(p) (char*)(((size_t)(p) + (TL_REGION_MAX_ALIGN-1)) & ~((size_t)TL_REGION_MAX_ALIGN - 1))

void* tl_region_alloc_newblock(tl_region* r, size_t rounded_size) {
	tl_region_block* bl = malloc(4096);
	char* ret;

	bl->next = r->first_block;
	r->first_block = bl;
	r->cur = ALIGN_PTR(bl->mem);
	r->end = (char*)((size_t)bl + 4096);

	ret = r->cur;
	r->cur += rounded_size;
	return ret;
}

void tl_region_free(tl_region* r) {
	tl_region_block* bl = r->first_block;

	while(bl) {
		tl_region_block* next = bl->next;
		free(bl);
		bl = next;
	}
}

#undef ALIGN_PTR

