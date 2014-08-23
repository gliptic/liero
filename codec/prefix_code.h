#ifndef UUID_49F7864571834C62DC0420B330A7D57C
#define UUID_49F7864571834C62DC0420B330A7D57C

#include "../config.h"
//#include "../platform.h"
#include "../cstdint.h"

#define TL_MAX_SUPPORTED_SYMS (1024)

typedef struct tl_ord_freq
{
	uint16 sym, freq;
} tl_ord_freq;

void tl_sort_symbols(tl_ord_freq* symbols, uint32 num_syms);
int tl_generate_codes(unsigned num_syms, uint8 const* code_sizes, uint16* codes);
int tl_limit_max_code_size(uint8* code_sizes, unsigned num_syms, unsigned max_code_size);

#endif // UUID_49F7864571834C62DC0420B330A7D57C
