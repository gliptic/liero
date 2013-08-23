#ifndef UUID_A0E64B040F4F41B4EC933B89A48C42C7
#define UUID_A0E64B040F4F41B4EC933B89A48C42C7

#include "../support/cstdint.hpp"

/* NOTE: Keep this usable from C */

#ifdef __cplusplus
extern "C" {
#endif

// A timer incrementing once per millisecond
uint32_t gvl_get_ticks();

// A timer with higher precision, incrementing hires_ticks_per_sec() ticks
// per second.
// NOTE: The precision isn't necessarily hires_ticks_per_sec()
// per second.
uint64_t gvl_get_hires_ticks();
uint64_t gvl_hires_ticks_per_sec();

void gvl_sleep(uint32_t ms);

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus
namespace gvl
{

// A timer incrementing once per millisecond
GVL_INLINE uint32_t get_ticks() { return gvl_get_ticks(); }

// A timer with higher precision, incrementing hires_ticks_per_sec() ticks
// per second.
// NOTE: The precision isn't necessarily hires_ticks_per_sec()
// per second.
GVL_INLINE uint64_t get_hires_ticks() { return gvl_get_hires_ticks(); }
GVL_INLINE uint64_t hires_ticks_per_sec() { return gvl_hires_ticks_per_sec(); }

GVL_INLINE void sleep(uint32_t ms) { return gvl_sleep(ms); }

}
#endif

#endif // UUID_A0E64B040F4F41B4EC933B89A48C42C7

