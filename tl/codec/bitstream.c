#include "bitstream.h"

void tl_bitsink_putbits(tl_bitsink* s, uint32 b, int32 n)
{
	assert(n > 0);
more:
	s->bits |= (b << (32-n)) >> s->bits_written;
	if((s->bits_written += n) >= 32 - TL_BITSINK_OFFSET)
	{
		n = s->bits_written - (32 - TL_BITSINK_OFFSET);
		tl_bs_push32(&s->sink, s->bits);
		s->bits_written = -TL_BITSINK_OFFSET;
		s->bits = 0;
		if(n) goto more;
	}
}

void tl_bitsink_flush(tl_bitsink* s)
{
	if(s->bits_written > -TL_BITSINK_OFFSET)
	{
		tl_bs_push32(&s->sink, s->bits);
		s->bits_written = -TL_BITSINK_OFFSET;
		s->bits = 0;
	}
}

void tl_bitsink_flushbytes(tl_bitsink* s)
{
	while(s->bits_written > -TL_BITSINK_OFFSET)
	{
		tl_bs_push(&s->sink, s->bits >> 24);
		s->bits <<= 8;
		s->bits_written -= 8;
	}

	s->bits_written = -TL_BITSINK_OFFSET;
	assert(s->bits == 0);
}
