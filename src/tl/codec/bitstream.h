#ifndef UUID_90716EDD05C149B60BBD2BA216A2C07C
#define UUID_90716EDD05C149B60BBD2BA216A2C07C

#include "../config.h"
#include "../cstdint.h"
#include "../stream.h"
#include "../platform.h"
#include <assert.h>

typedef struct tl_bitsink
{
	uint32 bits;
	int32  bits_written;

	tl_byte_sink_pushable sink;
} tl_bitsink;

typedef struct tl_bitsource
{
	uint32 bits;
	int32  bits_read;

	tl_byte_source_pullable source;
} tl_bitsource;

// More efficient to offset the count if we can
#define TL_BITSINK_OFFSET (TL_MASKED_SHIFT_COUNT ? 32 : 0)

TL_INLINE void tl_bitsink_init(tl_bitsink* s)
{
	s->bits = 0;
	s->bits_written = -TL_BITSINK_OFFSET;
}

TL_INLINE void tl_bitsink_deinit(tl_bitsink* s)
{
	tl_bs_free_sink(&s->sink);
}

TL_INLINE void tl_bitsink_put(tl_bitsink* s, uint32 b)
{
	assert((b&~1u) == 0);
	s->bits |= b << (31 - s->bits_written++);
	if(s->bits_written >= 32 - TL_BITSINK_OFFSET)
	{
		tl_bs_push32(&s->sink, s->bits);
		s->bits = 0;
		s->bits_written = -TL_BITSINK_OFFSET;
	}
}

void tl_bitsink_putbits(tl_bitsink* s, uint32 b, int32 n);
void tl_bitsink_flush(tl_bitsink* s);
void tl_bitsink_flushbytes(tl_bitsink* s);

TL_INLINE void tl_bitsource_init(tl_bitsource* s)
{
	s->bits = 0;
	s->bits_read = 0;
}

#endif // UUID_90716EDD05C149B60BBD2BA216A2C07C
