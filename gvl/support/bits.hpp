#ifndef UUID_D006EF6EB7A24020D1926ABC53D805D6
#define UUID_D006EF6EB7A24020D1926ABC53D805D6

#include "cstdint.hpp"
#include "platform.hpp"

#if GVL_MSVCPP
# include <stdlib.h>
# include <intrin.h>
# include <limits.h>
# pragma intrinsic(_BitScanReverse)
# pragma intrinsic(_BitScanForward)
# if GVL_X86_64
#  pragma intrinsic(_BitScanReverse64)
# endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if GVL_MSVCPP
GVL_INLINE int gvl_log2(uint32_t v)
{
	unsigned long r;
	if(!_BitScanReverse(&r, v))
		r = 0;
	return r;
}

GVL_INLINE int gvl_top_bit(uint32_t v)
{
	unsigned long r;
	if(!_BitScanReverse(&r, v))
		return -1;
	return r;
}

GVL_INLINE int gvl_bottom_bit(uint32_t v)
{
	unsigned long r;
	if(!_BitScanForward(&r, v))
		return -1;
	return r;
}

GVL_INLINE int gvl_log2_64(uint64_t v)
{
	unsigned long r = 0;
#if GVL_X86_64
	if(!_BitScanReverse64(&r, v))
		r = 0;
#else
	if(_BitScanReverse(&r, (uint32_t)(v >> 32)))
		return 32 + r;
	if(!_BitScanReverse(&r, (uint32_t)v))
		return 0;
#endif
	return r;
}

GVL_INLINE int gvl_trailing_zeroes(uint32_t v)
{
	unsigned long r;
	if(!_BitScanForward(&r, v))
		return 0;
	return r;
}

GVL_INLINE uint32_t gvl_bswap(uint32_t v)
{
	int const ulong_shift = (sizeof(unsigned long) - sizeof(uint32_t)) * CHAR_BIT;
	
	return (uint32_t)(_byteswap_ulong((unsigned long)v << ulong_shift));
}

GVL_INLINE uint64_t gvl_bswap_64(uint64_t v)
{
	return _byteswap_uint64(v);
}

#else
int gvl_log2(uint32_t v);
int gvl_log2_64(uint64_t v);
int gvl_top_bit(uint32_t v);
int gvl_bottom_bit(uint32_t v);
int gvl_trailing_zeroes(uint32_t v);

GVL_INLINE uint32_t gvl_bswap(uint32_t v)
{
	return (v >> 24)
	| ((v >> 8) & 0xff00)
	| ((v << 8) & 0xff0000)
	| ((v << 24) & 0xff000000);
}

GVL_INLINE uint64_t gvl_bswap_64(uint64_t v)
{
	return gvl_bswap((uint32_t)(v >> 32)) | ((uint64_t)gvl_bswap((uint32_t)v) << 32);
}

#endif

GVL_INLINE uint32_t gvl_bswap_le(uint32_t v)
{
#if GVL_LITTLE_ENDIAN
	return v;
#else
	return gvl_bswap(v);
#endif
}

GVL_INLINE uint32_t gvl_bswap_be(uint32_t v)
{
#if GVL_LITTLE_ENDIAN
	return gvl_bswap(v);
#else
	return v;
#endif
}

GVL_INLINE uint32_t gvl_rot(uint32_t v, int c)
{
	return (v << c) | (v >> (32-c));
}

GVL_INLINE uint64_t gvl_rot_64(uint64_t v, int c)
{
	return (v << c) | (v >> (64-c));
}

GVL_INLINE int gvl_popcount(uint32_t v)
{
	v = v - ((v >> 1) & 0x55555555);                    /* reuse input as temporary */
	v = (v & 0x33333333) + ((v >> 2) & 0x33333333);     /* temp */
	return (((v + (v >> 4)) & 0xF0F0F0F) * 0x1010101) >> 24; /* count */
}

GVL_INLINE int gvl_ceil_log2(uint32_t v)
{
	return v == 0 ? 0 : gvl_log2(v - 1) + 1;
}

GVL_INLINE int gvl_even_log2(uint32_t v)
{
	/* TODO: Special look-up table for this */
	return ((gvl_log2(v) + 1) & ~1);
}

GVL_INLINE int gvl_odd_log2(uint32_t v)
{
	return (gvl_log2(v) | 1);
}

GVL_INLINE int gvl_odd_log2_64(uint64_t v)
{
	return (gvl_log2_64(v) | 1);
}

#if GVL_SIGN_EXTENDING_RIGHT_SHIFT && GVL_TWOS_COMPLEMENT

/* Returns v if v >= 0, otherwise 0 */
GVL_INLINE int32_t gvl_saturate0(int32_t v)
{
	return (v & ~(v >> 31));
}

#else

/* Returns v if v >= 0, otherwise 0 */
GVL_INLINE int32_t gvl_saturate0(int32_t v)
{
	return v < 0 ? 0 : v;
}

#endif

GVL_INLINE int32_t gvl_udiff(uint32_t x, uint32_t y)
{
	x -= y;
	return x < 0x80000000ul ? (int32_t)(x) : (int32_t)(x - 0x80000000ul) - 0x80000000;
}

/* Whether x is in the modulo 2^32 interval [b, e) */
GVL_INLINE int gvl_cyclic_between(uint32_t b, uint32_t e, uint32_t x)
{
	return (x - b) < (e - b);
}

GVL_INLINE int gvl_cyclic_between_mask(uint32_t b, uint32_t e, uint32_t x, uint32_t mask)
{
	return ((x - b) & mask) < ((e - b) & mask);
}

GVL_INLINE int32_t gvl_uint32_as_int32(uint32_t x)
{
	if(x >= 0x80000000)
		return (int32_t)(x - 0x80000000u) - 0x80000000;
	else
		return (int32_t)(x);
}

GVL_INLINE uint32_t gvl_int32_as_uint32(int32_t x)
{
	if(x < 0)
		return (uint32_t)(x + 0x80000000) + 0x80000000u;
	else
		return (uint32_t)(x);
}

/* lsb_mask(x) works for x in [1, 32] */
GVL_INLINE uint32_t gvl_lsb_mask(int x)
{
	return (~(uint32_t)(0)) >> (uint32_t)(32-x);
}

/* Left shift that works for o in [1, 32] */
GVL_INLINE uint32_t gvl_shl_1_32(uint32_t v, uint32_t o)
{
	return (v << (o - 1)) << 1;
}

/* Right shift that works for o in [1, 32] */
GVL_INLINE uint32_t gvl_shr_1_32(uint32_t v, uint32_t o)
{
	return (v >> (o - 1)) >> 1;
}

GVL_INLINE int gvl_all_set(uint32_t v, uint32_t f)
{
	return (v & f) == f;
}

GVL_INLINE int gvl_is_power_of_two(uint32_t x)
{
	return (x & (x-1)) == 0 && x != 0;
}

/*
void write_uint32(uint8_t* ptr, uint32_t v);
uint32_t read_uint32(uint8_t const* ptr);
void write_uint16(uint8_t* ptr, uint32_t v);
uint32_t read_uint16(uint8_t const* ptr);*/

#ifdef __cplusplus
} // extern "C"
#endif

#if defined(__cplusplus)
namespace gvl
{

GVL_INLINE int log2(uint32_t v) { return gvl_log2(v); }
GVL_INLINE int top_bit(uint32_t v) { return gvl_top_bit(v); }
GVL_INLINE int bottom_bit(uint32_t v) { return gvl_bottom_bit(v); }
GVL_INLINE int log2(uint64_t v) { return gvl_log2_64(v); }
GVL_INLINE int trailing_zeroes(uint32_t v) { return gvl_trailing_zeroes(v); }
GVL_INLINE uint32_t bswap(uint32_t v) { return gvl_bswap(v); }
GVL_INLINE uint64_t bswap(uint64_t v) { return gvl_bswap_64(v); }
GVL_INLINE uint32_t bswap_le(uint32_t v) { return gvl_bswap_le(v); }
GVL_INLINE uint32_t bswap_be(uint32_t v) { return gvl_bswap_be(v); }
GVL_INLINE int popcount(uint32_t v) { return gvl_popcount(v); }
GVL_INLINE int ceil_log2(uint32_t v) { return gvl_ceil_log2(v); }
GVL_INLINE int even_log2(uint32_t v) { return gvl_even_log2(v); }
GVL_INLINE int odd_log2(uint32_t v) { return gvl_odd_log2(v); }
GVL_INLINE int odd_log2(uint64_t v) { return gvl_odd_log2_64(v); }
GVL_INLINE int32_t saturate0(int32_t v) { return gvl_saturate0(v); }
GVL_INLINE int32_t udiff(uint32_t x, uint32_t y) { return gvl_udiff(x, y); }
GVL_INLINE bool cyclic_between(uint32_t b, uint32_t e, uint32_t x) { return gvl_cyclic_between(b, e, x) != 0; }
GVL_INLINE bool cyclic_between(uint32_t b, uint32_t e, uint32_t x, uint32_t mask) { return gvl_cyclic_between_mask(b, e, x, mask) != 0; }
GVL_INLINE int32_t uint32_as_int32(uint32_t x) { return gvl_uint32_as_int32(x); }
GVL_INLINE uint32_t int32_as_uint32(int32_t x) { return gvl_int32_as_uint32(x); }
GVL_INLINE uint32_t lsb_mask(int x) { return gvl_lsb_mask(x); }
GVL_INLINE uint32_t shl_1_32(uint32_t v, uint32_t o) { return gvl_shl_1_32(v, o); }
GVL_INLINE uint32_t shr_1_32(uint32_t v, uint32_t o) { return gvl_shr_1_32(v, o); }
GVL_INLINE bool all_set(uint32_t v, uint32_t f) { return gvl_all_set(v, f) != 0; }
GVL_INLINE bool is_power_of_two(uint32_t x) { return gvl_is_power_of_two(x) != 0; }

}
#endif

#endif /* UUID_D006EF6EB7A24020D1926ABC53D805D6 */
