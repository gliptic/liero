#include "polar_code.h"

#include "../bits.h"
#include "../algo.h"
#include <assert.h>

unsigned tl_polar_code_lengths(tl_ord_freq const* symbols, uint32 num_syms, uint8* code_sizes)
{
	int tmp_freq[TL_POLAR_MAX_SYMBOLS];
	unsigned i, tree_total, tree_total_bits;
	unsigned orig_total_freq = 0;
	unsigned cur_total = 0;
	unsigned start_index = 0, max_code_size = 0;

	for (i = 0; i < num_syms; i++)
	{
		unsigned sym_freq = symbols[i].freq;
		unsigned sym_len = tl_top_bit(sym_freq) + 1;
		unsigned adjusted_sym_freq = 1 << (sym_len - 1);

		orig_total_freq += sym_freq;
		tmp_freq[i] = adjusted_sym_freq;
		cur_total += adjusted_sym_freq;
	}

	tree_total = 1 << tl_top_bit(orig_total_freq);
	if (tree_total < orig_total_freq)
		tree_total <<= 1;

	while ((cur_total < tree_total) && (start_index < num_syms))
	{
		for (i = start_index; i < num_syms; i++)
		{
			unsigned freq = tmp_freq[i];
			if ((cur_total + freq) <= tree_total)
			{
				tmp_freq[i] += freq;
				if ((cur_total += freq) == tree_total)
					break;
			}
			else
			{
				start_index = i + 1;
			}
		}
	}

	assert(cur_total == tree_total);

	tree_total_bits = tl_top_bit(tree_total) + 1;
	for (i = 0; i < num_syms; i++)
	{
		unsigned codesize = (tree_total_bits - tl_top_bit(tmp_freq[i]) - 1);
		max_code_size = tl_max(codesize, max_code_size);
		code_sizes[symbols[i].sym] = (uint8)codesize;
	}

	return max_code_size;
}