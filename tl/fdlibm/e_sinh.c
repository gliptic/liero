
/* @(#)e_sinh.c 1.3 95/01/18 */
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

/* _ieee754_sinh(x)
 * Method :
 * mathematically fd_sinh(x) if defined to be (fd_exp(x)-fd_exp(-x))/2
 *	1. Replace x by |x| (fd_sinh(-x) = -fd_sinh(x)).
 *	2.
 *		                                    E + E/(E+1)
 *	    0        <= x <= 22     :  fd_sinh(x) := --------------, E=fd_expm1(x)
 *			       			        2
 *
 *	    22       <= x <= lnovft :  fd_sinh(x) := fd_exp(x)/2
 *	    lnovft   <= x <= ln2ovft:  fd_sinh(x) := fd_exp(x/2)/2 * fd_exp(x/2)
 *	    ln2ovft  <  x	    :  fd_sinh(x) := x*shuge (overflow)
 *
 * Special cases:
 *	fd_sinh(x) is |x| if x is +INF, -INF, or NaN.
 *	only fd_sinh(0)=0 is exact for fd_finite x.
 */

#include "fdlibm.h"
#include "fdlibm_intern.h"


