
/* @(#)e_acosh.c 1.3 95/01/18 */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 *
 */

/* _ieee754_acosh(x)
 * Method :
 *	Based on
 *		fd_acosh(x) = fd_log [ x + fd_sqrt(x*x-1) ]
 *	we have
 *		fd_acosh(x) := fd_log(x)+ln2,	if x is large; else
 *		fd_acosh(x) := fd_log(2x-1/(fd_sqrt(x*x-1)+x)) if x>2; else
 *		fd_acosh(x) := fd_log1p(t+fd_sqrt(2.0*t+t*t)); where t=x-1.
 *
 * Special cases:
 *	fd_acosh(x) is NaN with signal if x<1.
 *	fd_acosh(NaN) is NaN without signal.
 */

#include "fdlibm.h"
#include "fdlibm_intern.h"
