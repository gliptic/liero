
/* @(#)e_log10.c 1.3 95/01/18 */
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

/* _ieee754_log10(x)
 * Return the base 10 logarithm of x
 *
 * Method :
 *	Let log10_2hi = leading 40 bits of fd_log10(2) and
 *	    log10_2lo = fd_log10(2) - log10_2hi,
 *	    ivln10   = 1/fd_log(10) rounded.
 *	Then
 *		n = fd_ilogb(x),
 *		if(n<0)  n = n+1;
 *		x = fd_scalbn(x,-n);
 *		fd_log10(x) := n*log10_2hi + (n*log10_2lo + ivln10*fd_log(x))
 *
 * Note 1:
 *	To guarantee fd_log10(10**n)=n, where 10**n is normal, the rounding
 *	mode must set to Round-to-Nearest.
 * Note 2:
 *	[1/fd_log(10)] rounded to 53 bits has error  .198   ulps;
 *	fd_log10 is monotonic at all binary break points.
 *
 * Special cases:
 *	fd_log10(x) is NaN with signal if x < 0;
 *	fd_log10(+INF) is +INF with no signal; fd_log10(0) is -INF with signal;
 *	fd_log10(NaN) is that NaN with no signal;
 *	fd_log10(10**N) = N  for N=0,1,...,22.
 *
 * Constants:
 * The hexadecimal values are the intended ones for the following constants.
 * The decimal values may be used, provided that the compiler will convert
 * from decimal to binary accurately enough to produce the hexadecimal values
 * shown.
 */

#include "fdlibm.h"
#include "fdlibm_intern.h"
