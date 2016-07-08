#ifndef UUID_728FDBF69FF04497D59A6F8E9506DF64
#define UUID_728FDBF69FF04497D59A6F8E9506DF64

#include <stddef.h>

#if !defined(TL_CPP)
#if defined(__cplusplus)
#define TL_CPP 1
#else
#define TL_CPP 0
#endif
#endif

#if !defined(TL_WIN64)
# if defined(WIN64) || defined(_WIN64) /* TODO: Check for Cygwin */
#  define TL_WIN64 1
# else
#  define TL_WIN64 0
# endif
#endif

#if !defined(TL_WIN32)
# if defined(WIN32) || defined(_WIN32) /* TODO: Check for Cygwin */
#  define TL_WIN32 1
# else
#  define TL_WIN32 0
# endif
#endif

#if !defined(TL_WINDOWS)
# define TL_WINDOWS (TL_WIN32 || TL_WIN64)
#endif

#if !defined(TL_MSVCPP)
# if defined(_MSC_VER)
#  define TL_MSVCPP _MSC_VER
# else
#  define TL_MSVCPP 0
# endif
#endif

#if !defined(TL_GCC)
# if defined(__GNUC__)
#  define TL_GCC 1
# else
#  define TL_GCC 0
# endif
#endif

#if !defined(TL_LINUX)
# if defined(__linux__) || defined(linux)
#  define TL_LINUX 1
# else
#  define TL_LINUX 0
# endif
#endif

#if !TL_X86 && !TL_X86_64
# if defined(_M_X64) || defined(__x86_64__) || TL_WIN64
#  define TL_X86_64 1
# elif defined(__i386__) || defined(_M_IX86) || defined(i386) || defined(i486) || defined(intel) || defined(x86) || defined(i86pc)
#  define TL_X86 1
# else
#  error "Unknown architecture, please add it"
# endif
#endif

#if !TL_LITTLE_ENDIAN && !TL_BIG_ENDIAN
# if TL_X86 || TL_X86_64
#  define TL_LITTLE_ENDIAN 1
# else
#  define TL_BIG_ENDIAN 1
# endif
#endif

#if defined(__cplusplus) // We don't test TL_CPP here, because we assume we can always use C++ inline in C++
# define TL_INLINE inline
# if TL_MSVCPP
#  define TL_FORCE_INLINE __forceinline
# else
#  define TL_FORCE_INLINE inline
# endif
#elif TL_GCC
# define TL_INLINE static inline
#elif TL_MSVCPP
# define TL_INLINE __inline
# define TL_FORCE_INLINE __forceinline
#else
# define TL_INLINE static
#endif

#ifndef TL_FORCE_INLINE
#define TL_FORCE_INLINE TL_INLINE
#endif

#if !defined(TL_CPP0X)
# if TL_MSVCPP >= 1600
#  define TL_CPP0X 1 // C++0x level 1
# else
#  define TL_CPP0X 0
# endif
#endif

#if !defined(TL_STDC)
# if __STDC__
#  define TL_STDC __STDC_VERSION__
# else
#  define TL_STDC 199409L 
# endif
#endif

/* Whether or not the compiler may generate x87 code for floating point calculations.
** TL_X87 == 1 means the tl IEEE support functions will take measures to work-around
** x87 issues that make results non-reproducible. */
#if !defined(TL_X87)
# if TL_X86
#  define TL_X87 1 // Assume the compiler generates x87 code on x86 unless otherwise stated
# elif TL_X86_64
#  define TL_X87 0 // SSE2 is typically used on TL_X86_64
# endif
#endif

/* Whether or not types can be read from unaligned addresses */
#if !defined(TL_UNALIGNED_ACCESS)
# if TL_X86 || TL_X86_64
#  define TL_UNALIGNED_ACCESS 1
# else
#  define TL_UNALIGNED_ACCESS 0
# endif
#endif

/* At least x86 and x86_64 provide efficient masked shift counts in shifts */
#if !defined(TL_MASKED_SHIFT_COUNT)
# if TL_X86 || TL_X86_64
#  define TL_MASKED_SHIFT_COUNT 1
# else
#  define TL_MASKED_SHIFT_COUNT 0
# endif
#endif

#ifndef TL_DIV_ROUNDS_TOWARD_ZERO
#define TL_DIV_ROUNDS_TOWARD_ZERO ((-1/2)==0)
#endif

#ifndef TL_SIGN_EXTENDING_RIGHT_SHIFT
#define TL_SIGN_EXTENDING_RIGHT_SHIFT (((-1)>>15)==-1)
#endif

#ifndef TL_TWOS_COMPLEMENT
#define TL_TWOS_COMPLEMENT (~(-1)==0)
#endif


#if !defined(TL_WINDOWS)
#define TL_WINDOWS (TL_WIN64 || TL_WIN32)
#endif

#if !defined(TL_PTR64)
# if TL_WIN64
#  define TL_PTR64 1
# else
#  define TL_PTR64 0
# endif
#endif

// Taken from LuaJIT
#define TL_ASSERT_NAME2(name, line)	name ## line
#define TL_ASSERT_NAME(line) TL_ASSERT_NAME2(tl_assert_, line)
#ifdef __COUNTER__
# define TL_STATIC_ASSERT(cond) \
   extern void TL_ASSERT_NAME(__COUNTER__)(int STATIC_ASSERTION_FAILED[(cond)?1:-1])
#else
# define TL_STATIC_ASSERT(cond) \
   extern void TL_ASSERT_NAME(__LINE__)(int STATIC_ASSERTION_FAILED[(cond)?1:-1])
#endif

#define TL_U64x(hi, lo) (((uint64_t)0x##hi << 32) + (uint64_t)0x##lo)

#endif // UUID_728FDBF69FF04497D59A6F8E9506DF64

