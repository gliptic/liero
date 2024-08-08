#include "prefix_code.h"

#include "../bits.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int sym_comp(void const* a, void const* b) {
	int af = ((tl_ord_freq const*)a)->freq;
	int bf = ((tl_ord_freq const*)b)->freq;
	return af < bf ? 1 : (af > bf ? -1 : 0);
}

void tl_sort_symbols(tl_ord_freq* symbols, uint32 num_syms) {
	qsort(symbols, num_syms, sizeof(tl_ord_freq), sym_comp);
}

#define TL_MAX_EXPECTED_CODE_SIZE (16)

int tl_generate_codes(unsigned num_syms, uint8 const* code_sizes, uint16* codes) {
	unsigned i;
	unsigned num_codes[TL_MAX_EXPECTED_CODE_SIZE + 1];
	unsigned next_code[TL_MAX_EXPECTED_CODE_SIZE + 1];
	unsigned code = 0;
	memset(num_codes, 0, sizeof(num_codes));

	for(i = 0; i < num_syms; i++) {
		unsigned c = code_sizes[i];
		assert(c <= TL_MAX_EXPECTED_CODE_SIZE);
		num_codes[c]++;
	}

	next_code[0] = 0;

	for(i = 1; i <= TL_MAX_EXPECTED_CODE_SIZE; i++)	{
		next_code[i] = code;
		code = (code + num_codes[i]) << 1;
	}

	if(code != (1 << (TL_MAX_EXPECTED_CODE_SIZE + 1))) {
		unsigned t = 0;
		for (i = 1; i <= TL_MAX_EXPECTED_CODE_SIZE; i++) {
			t += num_codes[i];
			if (t > 1)
				return 0;
		}
	}

	for(i = 0; i < num_syms; i++) {
		unsigned c = code_sizes[i];
		assert(!c || (next_code[c] <= 0xffff));

		codes[i] = (uint16)(next_code[c]++);
		assert(!c || (tl_top_bit(codes[i]) + 1 <= code_sizes[i]));
	}

	return 1;
}

#define TL_MAX_EVER_CODE_SIZE (34)

int tl_limit_max_code_size(uint8* code_sizes, unsigned num_syms, unsigned max_code_size) {
	unsigned i;
	uint8 new_codesizes[TL_MAX_SUPPORTED_SYMS];
	unsigned num_codes[TL_MAX_EVER_CODE_SIZE + 1];
	unsigned ofs = 0, total = 0;
	unsigned next_sorted_ofs[TL_MAX_EVER_CODE_SIZE + 1];
	int should_limit = 0;
	uint8* p = new_codesizes;

	if ((!num_syms) || (num_syms > TL_MAX_SUPPORTED_SYMS)
	|| (max_code_size < 1) || (max_code_size > TL_MAX_EVER_CODE_SIZE))
		return 0;

	memset(num_codes, 0, sizeof(num_codes));

	for(i = 0; i < num_syms; i++) {
		unsigned c = code_sizes[i];
		assert(c <= TL_MAX_EVER_CODE_SIZE);

		num_codes[c]++;
		if (c > max_code_size)
			should_limit = 1;
	}

	if(!should_limit)
		return 1;

	for(i = 1; i <= TL_MAX_EVER_CODE_SIZE; ++i) {
		next_sorted_ofs[i] = ofs;
		ofs += num_codes[i];
	}

	if ((ofs < 2) || (ofs > TL_MAX_SUPPORTED_SYMS))
		return 1;

	if (ofs > (1U << max_code_size))
		return 0;

	for(i = max_code_size + 1; i <= TL_MAX_EVER_CODE_SIZE; i++)
		num_codes[max_code_size] += num_codes[i];

	// Technique of adjusting tree to enforce maximum code size from LHArc.

	for(i = max_code_size; i; --i)
		total += (num_codes[i] << (max_code_size - i));

	if(total == (1U << max_code_size))
		return 1;

	do {
		num_codes[max_code_size]--;

		for (i = max_code_size - 1; i && !num_codes[i]; --i)
			/* empty */;

		if (!i) return 0;

		num_codes[i]--;
		num_codes[i + 1] += 2;
		total--;
	} while (total != (1U << max_code_size));

	for(i = 1; i <= max_code_size; i++) {
		unsigned n = num_codes[i];
		if(n) {
			memset(p, i, n);
			p += n;
		}
	}

	for(i = 0; i < num_syms; i++) {
		unsigned c = code_sizes[i];
		if(c) {
			unsigned ofs = next_sorted_ofs[c];
			next_sorted_ofs[c] = ofs + 1;

			code_sizes[i] = (uint8)(new_codesizes[ofs]);
		}
	}

	return 1;
}
