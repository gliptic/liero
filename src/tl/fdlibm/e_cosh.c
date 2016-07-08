
/* @(#)e_cosh.c 1.3 95/01/18 */
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

/* _ieee754_cosh(x)
 * Method :
 * mathematically fd_cosh(x) if defined to be (fd_exp(x)+fd_exp(-x))/2
 *	1. Replace x by |x| (fd_cosh(x) = fd_cosh(-x)).
 *	2.
 *		                                        [ fd_exp(x) - 1 ]^2
 *	    0        <= x <= ln2/2  :  fd_cosh(x) := 1 + -------------------
 *			       			           2*fd_exp(x)
 *
 *		                                  fd_exp(x) +  1/fd_exp(x)
 *	    ln2/2    <= x <= 22     :  fd_cosh(x) := -------------------
 *			       			          2
 *	    22       <= x <= lnovft :  fd_cosh(x) := fd_exp(x)/2
 *	    lnovft   <= x <= ln2ovft:  fd_cosh(x) := fd_exp(x/2)/2 * fd_exp(x/2)
 *	    ln2ovft  <  x	    :  fd_cosh(x) := huge*huge (overflow)
 *
 * Special cases:
 *	fd_cosh(x) is |x| if x is +INF, -INF, or NaN.
 *	only fd_cosh(0)=1 is exact for fd_finite x.
 */

#include "fdlibm.h"
#include "fdlibm_intern.h"
