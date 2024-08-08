
/* @(#)s_tanh.c 1.3 95/01/18 */
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

/* Tanh(x)
 * Return the Hyperbolic Tangent of x
 *
 * Method :
 *				       x    -x
 *				      e  - e
 *	0. fd_tanh(x) is defined to be -----------
 *				       x    -x
 *				      e  + e
 *	1. reduce x to non-negative by fd_tanh(-x) = -fd_tanh(x).
 *	2.  0      <= x <= 2**-55 : fd_tanh(x) := x*(one+x)
 *					        -t
 *	    2**-55 <  x <=  1     : fd_tanh(x) := -----; t = fd_expm1(-2x)
 *					       t + 2
 *						     2
 *	    1      <= x <=  22.0  : fd_tanh(x) := 1-  ----- ; t=fd_expm1(2x)
 *						   t + 2
 *	    22.0   <  x <= INF    : fd_tanh(x) := 1.
 *
 * Special cases:
 *	fd_tanh(NaN) is NaN;
 *	only fd_tanh(0)=0 is exact for fd_finite argument.
 */

#include "fdlibm.h"
#include "fdlibm_intern.h"

double fd_tanh(double x)
{
	double t,z;
	int jx,ix;

    /* High word of |x|. */
	jx = FD_HI(x);
	ix = jx&0x7fffffff;

    /* x is INF or NaN */
	if(ix>=0x7ff00000) {
	    if (jx>=0) return gA(gD(one,x), one);    /* fd_tanh(+-inf)=+-1 */
	    else       return gS(gD(one,x), one);    /* fd_tanh(NaN) = NaN */
	}

    /* |x| < 22 */
	if (ix < 0x40360000) {		/* |x|<22 */
	    if (ix<0x3c800000) 		/* |x|<2**-55 */
		return gM(x,gA(one, x));    	/* fd_tanh(small) = small */
	    if (ix>=0x3ff00000) {	/* |x|>=1  */
		t = fd_expm1(gM(two,fd_fabs(x)));
		z = gS(one, gD(two,gA(t, two)));
	    } else {
	        t = fd_expm1(gM(-two,fd_fabs(x)));
	        z = gD(-t, gA(t, two));
	    }
    /* |x| > 22, return +-1 */
	} else {
	    z = gS(one, tiny);		/* raised inexact flag */
	}
	return (jx>=0)? z: -z;
}
