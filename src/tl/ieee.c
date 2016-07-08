#include "ieee.h"


#if TL_MSVCPP
#include <fpieee.h>
#include <excpt.h>
#endif

#if TL_GCC && TL_LINUX
#include <fpu_control.h>
#endif
#include <float.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

//#if TL_X87

// Exact 80-bit floating point little endian representation of 2^(16383 - 1023)
extern unsigned char const scaleup[10] = {0,0,0,0,0,0,0,128,255,123};

// Exact 80-bit floating point little endian representation of 1 / 2^(16383 - 1023)
extern unsigned char const scaledown[10] = {0,0,0,0,0,0,0,128,255,3};


//#endif

void gvl_init_ieee()
{
#if !TL_X86_64
#if TL_MSVCPP || (TL_GCC && TL_WIN32)
// Nothing needs to be done for VC++ by default, but let's do it anyway to be sure.
	unsigned int const flags = _RC_NEAR | _PC_53 | _EM_INVALID | _EM_DENORMAL | _EM_ZERODIVIDE | _EM_OVERFLOW | _EM_UNDERFLOW | _EM_INEXACT;
    _control87(flags, _MCW_EM | _MCW_PC | _MCW_RC);
#elif TL_GCC && TL_LINUX
	fpu_control_t v = _FPU_DOUBLE | _FPU_MASK_IM | _FPU_MASK_DM | _FPU_MASK_ZM | _FPU_MASK_OM | _FPU_MASK_UM | _FPU_MASK_PM | _FPU_RC_NEAREST;
	_FPU_SETCW(v);
#else
#  error "Don't know what to do on this platform"
#endif
#endif
}
