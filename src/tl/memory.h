#ifndef UUID_E4C54B1B70704E6FE73DE99BC631C8A4
#define UUID_E4C54B1B70704E6FE73DE99BC631C8A4

// Adapted from PortAudio

#if defined(__APPLE__)
#   include <libkern/OSAtomic.h>
    /* Here are the memory barrier functions. Mac OS X only provides
       full memory barriers, so the three types of barriers are the same,
       however, these barriers are superior to compiler-based ones. */
#   define TL_FULL_SYNC()  OSMemoryBarrier()
#   define TL_READ_SYNC()  OSMemoryBarrier()
#   define TL_WRITE_SYNC() OSMemoryBarrier()
#elif defined(__GNUC__)
    /* GCC >= 4.1 has built-in intrinsics. We'll use those */
#   if (__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 1)
#      define TL_FULL_SYNC()  __sync_synchronize()
#      define TL_READ_SYNC()  __sync_synchronize()
#      define TL_WRITE_SYNC() __sync_synchronize()
    /* as a fallback, GCC understands volatile asm and "memory" to mean it
     * should not reorder memory read/writes */
    /* Note that it is not clear that any compiler actually defines __PPC__,
     * it can probably be safely removed. */
#   elif defined( __ppc__ ) || defined( __powerpc__) || defined( __PPC__ )
#      define TL_FULL_SYNC()  asm volatile("sync":::"memory")
#      define TL_READ_SYNC()  asm volatile("sync":::"memory")
#      define TL_WRITE_SYNC() asm volatile("sync":::"memory")
#   elif defined( __i386__ ) || defined( __i486__ ) || defined( __i586__ ) || \
         defined( __i686__ ) || defined( __x86_64__ )
#      define TL_FULL_SYNC()  asm volatile("mfence":::"memory")
#      define TL_READ_SYNC()  asm volatile("lfence":::"memory")
#      define TL_WRITE_SYNC() asm volatile("sfence":::"memory")
#   else
#      ifdef ALLOW_SMP_DANGERS
#         warning Memory barriers not defined on this system or system unknown
#         warning For SMP safety, you should fix this.
#         define TL_FULL_SYNC() (void)0
#         define TL_READ_SYNC() (void)0
#         define TL_WRITE_SYNC() (void)0
#      else
#         error Memory barriers are not defined on this system. You can still compile by defining ALLOW_SMP_DANGERS, but SMP safety will not be guaranteed.
#      endif
#   endif
#elif (_MSC_VER >= 1400) && !defined(_WIN32_WCE)
#   include <intrin.h>
#   pragma intrinsic(_ReadWriteBarrier)
#   pragma intrinsic(_ReadBarrier)
#   pragma intrinsic(_WriteBarrier)
#   define TL_FULL_SYNC()  _ReadWriteBarrier()
#   define TL_READ_SYNC()  _ReadBarrier()
#   define TL_WRITE_SYNC() _WriteBarrier()
#elif defined(_WIN32_WCE)
#   define TL_FULL_SYNC() (void)0
#   define TL_READ_SYNC() (void)0
#   define TL_WRITE_SYNC() (void)0
#elif defined(_MSC_VER) || defined(__BORLANDC__)
#   define TL_FULL_SYNC()  _asm { lock add    [esp], 0 }
#   define TL_READ_SYNC()  _asm { lock add    [esp], 0 }
#   define TL_WRITE_SYNC() _asm { lock add    [esp], 0 }
#else
#   ifdef ALLOW_SMP_DANGERS
#      warning Memory barriers not defined on this system or system unknown
#      warning For SMP safety, you should fix this.
#      define TL_FULL_SYNC() (void)0
#      define TL_READ_SYNC() (void)0
#      define TL_WRITE_SYNC() (void)0
#   else
#      error Memory barriers are not defined on this system. You can still compile by defining ALLOW_SMP_DANGERS, but SMP safety will not be guaranteed.
#   endif
#endif

#endif // UUID_E4C54B1B70704E6FE73DE99BC631C8A4
