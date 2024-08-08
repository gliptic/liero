
/* @(#)s_frexp.c 1.4 95/01/18 */
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
 * for non-zero x
 *	x = fd_frexp(arg,&fd_exp);
 * return a double fp quantity x such that 0.5 <= |x| <1.0
 * and the corresponding binary exponent "fd_exp". That is
 *	arg = x*2^fd_exp.
 * If arg is inf, 0.0, or NaN, then fd_frexp(arg,&fd_exp) returns arg
 * with *fd_exp=0.
 */

#include "fdlibm.h"
#include "fdlibm_intern.h"

double fd_frexp(double x, int *eptr)
{
	int  hx, ix, lx;
	hx = FD_HI(x);
	ix = 0x7fffffff&hx;
	lx = FD_LO(x);
	*eptr = 0;
	if(ix>=0x7ff00000||((ix|lx)==0)) return x;	/* 0,inf,nan */
	if (ix<0x00100000) {		/* subnormal */
	    x = gM(x,two54);
	    hx = FD_HI(x);
	    ix = hx&0x7fffffff;
	    *eptr = -54;
	}
	*eptr += (ix>>20)-1022;
	hx = (hx&0x800fffff)|0x3fe00000;
	FD_HI(x) = hx;
	return x;
}
