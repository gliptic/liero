
/* @(#)s_lib_version.c 1.3 95/01/18 */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */

/*
 * MACRO for standards
 */

#include "fdlibm.h"
#include "fdlibm_intern.h"

/*
 * define and initialize FD_LIB_VERSION
 */
#ifdef _POSIX_MODE
FD_LIB_VERSION_TYPE FD_LIB_VERSION = FD_POSIX_;
#else
#ifdef _XOPEN_MODE
FD_LIB_VERSION_TYPE FD_LIB_VERSION = FD_XOPEN_;
#else
#ifdef _SVID3_MODE
FD_LIB_VERSION_TYPE FD_LIB_VERSION = FD_SVID_;
#else					/* default FD_IEEE_MODE */
FD_LIB_VERSION_TYPE FD_LIB_VERSION = FD_IEEE_;
#endif
#endif
#endif
