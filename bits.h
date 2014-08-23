#ifndef UUID_644FCF12367F4D475780169F9A4EAE7C
#define UUID_644FCF12367F4D475780169F9A4EAE7C

#include <stdlib.h>
#include "platform.h"
#include "cstdint.h"

#if TL_MSVCPP
# include <intrin.h>
# pragma intrinsic(_BitScanReverse)
# pragma intrinsic(_BitScanForward)
# if TL_X86_64
#  pragma intrinsic(_BitScanReverse64)
# endif

unsigned short   _byteswap_ushort(unsigned short   val);
unsigned long    _byteswap_ulong (unsigned long    val);
unsigned __int64 _byteswap_uint64(unsigned __int64 val);

# define tl_byteswap16(x) _byteswap_ushort(x)
# define tl_byteswap32(x) _byteswap_ulong(x)

TL_INLINE int tl_ffs(uint32_t x) {
	unsigned long r; _BitScanForward(&r, x); return r;
}

TL_INLINE int tl_fls(uint32_t x) {
	unsigned long r; _BitScanReverse(&r, x); return r;
}

TL_INLINE int tl_log2(uint32_t v) {
	unsigned long r;
	if(!_BitScanReverse(&r, v))
		r = 0;
	return r;
}

TL_INLINE int tl_top_bit(uint32_t v) {
	unsigned long r;
	if(!_BitScanReverse(&r, v))
		return -1;
	return r;
}

TL_INLINE int tl_bottom_bit(uint32_t v) {
	unsigned long r;
	if(!_BitScanForward(&r, v))
		return -1;
	return r;
}

#else // if !TL_MSVCPP

TL_INLINE uint16_t tl_byteswap16(uint16_t x) { return (x << 8) | (x >> 8); }
TL_INLINE uint32_t tl_byteswap32(uint32_t x) { return (x << 24) | (x >> 24) | ((x >> 8) & 0xff00) | ((x & 0xff00) << 8); }

// TODO: tl_log2, tl_top_bit, tl_bottom_bit, tl_ffs, tl_fls

#endif // elseif !TL_MSVCPP

#ifndef TL_HAS_FLS64
TL_INLINE int tl_fls64(uint64_t x) {
	return (x>>32) ? 32 + (int32_t)tl_fls((uint32_t)(x>>32)) : (int32_t)tl_fls((uint32_t)x);
}
#endif

static int tl_reverse_bits16(int n) {
	n = ((n & 0xAAAA) >>  1) | ((n & 0x5555) << 1);
	n = ((n & 0xCCCC) >>  2) | ((n & 0x3333) << 2);
	n = ((n & 0xF0F0) >>  4) | ((n & 0x0F0F) << 4);
	n = ((n & 0xFF00) >>  8) | ((n & 0x00FF) << 8);
	return n;
}

static unsigned int tl_reverse_bits32(unsigned int n) {
	n = ((n & 0xAAAAAAAA) >>  1) | ((n & 0x55555555) << 1);
	n = ((n & 0xCCCCCCCC) >>  2) | ((n & 0x33333333) << 2);
	n = ((n & 0xF0F0F0F0) >>  4) | ((n & 0x0F0F0F0F) << 4);
	n = ((n & 0xFF00FF00) >>  8) | ((n & 0x00FF00FF) << 8);
	return (n >> 16) | (n << 16);
}

#if TL_BIG_ENDIAN
# define tl_le32(x) tl_byteswap32(x)
# define tl_le16(x) tl_byteswap16(x)
# define tl_be32(x) (x)
# define tl_be16(x) (x)
#else
# define tl_le32(x) (x)
# define tl_le16(x) (x)
# define tl_be32(x) tl_byteswap32(x)
# define tl_be16(x) tl_byteswap16(x)
#endif

TL_INLINE uint32_t tl_rol32(uint32_t x, int s) {
	return (x << s) | (x >> (32-s));
}

TL_INLINE uint32_t tl_ror32(uint32_t x, int s) {
	return (x >> s) | (x << (32-s));
}

#endif // UUID_644FCF12367F4D475780169F9A4EAE7C
