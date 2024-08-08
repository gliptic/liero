#ifndef UUID_FF722730B6BE4516F104A0B79C227C9C
#define UUID_FF722730B6BE4516F104A0B79C227C9C

#include "prefix_code.h"

typedef struct tl_sym_freq
{
	uint32 freq;
	uint16 bits;
	uint8  length;
} tl_sym_freq;

typedef struct tl_model
{
	int n;
	uint32 sum, next_rebuild;
	tl_sym_freq symbols[1];
} tl_model;

#define TL_MODEL_SIZE(n) (sizeof(tl_model) + (n)*sizeof(tl_sym_freq) - sizeof(tl_sym_freq))

void tl_model_init_(tl_model* self, int size, int n);

#define tl_model_incr(self, sym) do { \
	tl_model* selfincr_ = (tl_model*)(&(self)[0]); \
	++selfincr_->symbols[sym].freq; \
	++selfincr_->sum; \
} while(0)

#define tl_model_update(self, rebuild) do { \
	tl_model* self_ = (tl_model*)(&(self)[0]); \
	if(self_->sum >= self_->next_rebuild) \
		rebuild(self_); \
} while(0)

#define tl_model_write(self, sink, sym) do { \
	tl_model* self_ = (tl_model*)(&(self)[0]); \
	int sym_ = (sym); \
	tl_bitsink_putbits((sink), self_->symbols[sym_].bits, self_->symbols[sym_].length); \
	tl_model_incr(self_, sym_); \
} while(0)

#define tl_model_init(self, num) do { \
	tl_model* self_ = (tl_model*)(&(self)[0]); \
	int num_ = (num); \
	tl_model_init_(self_, TL_MODEL_SIZE(num_), num_); \
} while(0)

#endif // UUID_FF722730B6BE4516F104A0B79C227C9C
