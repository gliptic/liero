
/* @(#)e_asin.c 1.3 95/01/18 */
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

/* _ieee754_asin(x)
 * Method :
 *	Since  fd_asin(x) = x + x^3/6 + x^5*3/40 + x^7*15/336 + ...
 *	we approximate fd_asin(x) on [0,0.5] by
 *		fd_asin(x) = x + x*x^2*R(x^2)
 *	where
 *		R(x^2) is a rational approximation of (fd_asin(x)-x)/x^3
 *	and its remez error is bounded by
 *		|(fd_asin(x)-x)/x^3 - R(x^2)| < 2^(-58.75)
 *
 *	For x in [0.5,1]
 *		fd_asin(x) = pi/2-2*fd_asin(fd_sqrt((1-x)/2))
 *	Let y = (1-x), z = y/2, s := fd_sqrt(z), and pio2_hi+pio2_lo=pi/2;
 *	then for x>0.98
 *		fd_asin(x) = pi/2 - 2*(s+s*z*R(z))
 *			= pio2_hi - (2*(s+s*z*R(z)) - pio2_lo)
 *	For x<=0.98, let pio4_hi = pio2_hi/2, then
 *		f = hi part of s;
 *		c = fd_sqrt(z) - f = (z-f*f)/(s+f) 	...f+c=fd_sqrt(z)
 *	and
 *		fd_asin(x) = pi/2 - 2*(s+s*z*R(z))
 *			= pio4_hi+(pio4-2s)-(2s*z*R(z)-pio2_lo)
 *			= pio4_hi+(pio4-2f)-(2s*z*R(z)-(pio2_lo+2c))
 *
 * Special cases:
 *	if x is NaN, return x itself;
 *	if |x|>1, return NaN with invalid signal.
 *
 */


#include "fdlibm.h"
#include "fdlibm_intern.h"
