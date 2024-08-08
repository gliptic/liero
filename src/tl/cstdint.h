#ifndef UUID_B6BA276BE4584C6984F223B4F0C5267F
#define UUID_B6BA276BE4584C6984F223B4F0C5267F

#include <limits.h>

#include "platform.h"

#if TL_STDC >= 199901L || _MSC_VER > 1600
#include <stdint.h>
#elif TL_GCC
#include <inttypes.h>
#else /* !TL_GCC */

#if !TL_EXCLUDE_STDINT

#if CHAR_BIT == 8 && UCHAR_MAX == 0xff
typedef unsigned char      uint8_t;
typedef   signed char      int8_t;
#else
#error "Only 8-bit chars supported"
#endif

typedef unsigned int       uint_fast8_t;
typedef          int       int_fast8_t;

#if USHRT_MAX == 0xffff
typedef unsigned short     uint16_t;
typedef          short     int16_t;
#elif UINT_MAX == 0xffff
typedef unsigned int       uint16_t;
typedef          int       int16_t;
#else
#error "No suitable 16-bit type"
#endif

#if USHRT_MAX == 0xffffffff
typedef unsigned short     uint32_t;
typedef          short     int32_t;
#elif UINT_MAX == 0xffffffff
typedef unsigned int       uint32_t;
typedef          int       int32_t;
#elif ULONG_MAX == 0xffffffff
typedef unsigned long      uint32_t;
typedef          long      int32_t;
#else
#error "No suitable 32-bit type"
#endif

/* We found a 32-bit type above, int should do */
#if TL_X86 || TL_X86_64
/* long should match the register size */
typedef unsigned long    uint_fast16_t;
typedef          long    int_fast16_t;
typedef unsigned long    uint_fast32_t;
typedef          long    int_fast32_t;
#elif UINT_MAX < 0xffffffff
/* Have to use long for 32-bit */
typedef unsigned int     uint_fast16_t;
typedef          int     int_fast16_t;
typedef unsigned long    uint_fast32_t;
typedef          long    int_fast32_t;
#else
/* Be conservative with space */
typedef unsigned int     uint_fast16_t;
typedef          int     int_fast16_t;
typedef unsigned int     uint_fast32_t;
typedef          int     int_fast32_t;
#endif

#define IS_64(x) ((x) > 0xffffffff && (x) == 0xffffffffffffffff)

#if IS_64(ULONG_MAX)
typedef unsigned long      uint64_t;
typedef          long      int64_t;
#elif defined(ULLONG_MAX) && IS_64(ULLONG_MAX)
typedef unsigned long long uint64_t;
typedef          long long int64_t;
#else
#error "No suitable 64-bit type"
#endif

#if defined(ULLONG_MAX)
typedef unsigned long long uintmax_t;
typedef          long long intmax_t;
#else
typedef unsigned long      uintmax_t;
typedef          long      intmax_t;
#endif

typedef uint64_t uint_fast64_t;
typedef int64_t int_fast64_t;
typedef ptrdiff_t intptr_t;

#endif

#define TL_BITS_IN(t) (sizeof(t)*CHAR_BIT)

#endif /* !TL_GCC */

typedef uint8_t  uint8;
typedef int8_t   int8;
typedef uint16_t uint16;
typedef int16_t  int16;
typedef uint32_t uint32;
typedef int32_t  int32;
typedef uint64_t uint64;
typedef int64_t  int64;

#endif // UUID_B6BA276BE4584C6984F223B4F0C5267F

