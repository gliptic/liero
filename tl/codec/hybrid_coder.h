#ifndef HYBRID_CODER_H
#define HYBRID_CODER_H

#include "../config.h"
#include "../cstdint.h"
#include "../stream.h"
#include "../platform.h"
#include "../vector.h"
#include <assert.h>

typedef struct tl_hybrid_sink {
	uint8 *buffer, *next, *end;
	uint8 *reserved_range_bytes[4];

	uint32 low, high;

	tl_byte_sink_pushable sink;
} tl_hybrid_sink;

TL_INLINE void tl_hybsink_init(tl_hybrid_sink* hs, size_t bufsize) {
	int i;
	assert(bufsize > 4);
	hs->next = hs->buffer = malloc(bufsize);
	hs->end = hs->next + bufsize;
	hs->low = 0;
	hs->high = 0xffffffff;

	for(i = 0; i < 4; ++i)
		hs->reserved_range_bytes[i] = hs->next++;
}

TL_INLINE void tl_hybsink_deinit(tl_hybrid_sink* hs) {
	free(hs->buffer);
	tl_bs_free_sink(&hs->sink);
}

TL_INLINE void tl_hybsink_flush(tl_hybrid_sink* hs) {
	int i;
	*hs->reserved_range_bytes[0] = (uint8)(hs->low >> 24);

	tl_bs_pushn(&hs->sink, hs->buffer, (hs->next - hs->buffer));
	hs->next = hs->buffer;
	hs->low = 0;
	hs->high = 0xffffffff;

	for(i = 0; i < 4; ++i)
		hs->reserved_range_bytes[i] = hs->next++;
}

TL_INLINE void tl_hybsink_reserve_range_byte(tl_hybrid_sink* hs) {

	hs->reserved_range_bytes[0] = hs->reserved_range_bytes[1];
	hs->reserved_range_bytes[1] = hs->reserved_range_bytes[2];
	hs->reserved_range_bytes[2] = hs->reserved_range_bytes[3];

	if(hs->next == hs->end) {
		tl_hybsink_flush(hs);
	} else {
		hs->reserved_range_bytes[3] = hs->next++;
	}
}

TL_INLINE void tl_hybsink_push_bit(tl_hybrid_sink* hs, int y, int p) {
	uint32 mid = hs->low + ((hs->high - hs->low) >> 12) * p;

	if(y) hs->high = mid;
	else hs->low = mid + 1;

	while((hs->low ^ hs->high) < (1<<24)) {
		*hs->reserved_range_bytes[0] = (uint8)(hs->high >> 24);
		tl_hybsink_reserve_range_byte(hs);
	}
}

TL_INLINE void tl_hybsink_push_byte(tl_hybrid_sink* hs, uint8 b) {
	if(hs->next == hs->end) {
		tl_hybsink_flush(hs);
	}
	*hs->next++ = b;
}

#endif // HYBRID_CODER_H
