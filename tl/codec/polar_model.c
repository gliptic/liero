#include "polar_model.h"
#include "polar_code.h"
#include "../bits.h"

void tl_polar_model_rebuild(tl_model* self)
{
	uint16 codes[256];
	uint8 lengths[256];
	tl_ord_freq symbols[256];
	unsigned max;
	int r, i;

	int shift = tl_top_bit(self->sum >> 16) + 1;
	int offset = (1 << shift) - 1;

	for(i = 0; i < self->n; ++i) {
		symbols[i].sym = i;
		symbols[i].freq = (self->symbols[i].freq + offset) >> shift;
	}

	tl_sort_symbols(symbols, self->n);
	max = tl_polar_code_lengths(symbols, self->n, lengths);

	r = tl_generate_codes(self->n, lengths, codes);

	for(i = 0; i < self->n; ++i) {
		self->symbols[i].bits = codes[i];
		self->symbols[i].length = lengths[i];
	}

	if(self->sum <= self->n)
		self->next_rebuild = self->n * 2;
	else
		self->next_rebuild = self->sum + 1024;
}
