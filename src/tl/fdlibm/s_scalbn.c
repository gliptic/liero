
/* @(#)s_scalbn.c 1.3 95/01/18 */
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
 * fd_scalbn (double x, int n)
 * fd_scalbn(x,n) returns x* 2**n  computed by  exponent
 * manipulation rather than by actually performing an
 * exponentiation or a multiplication.
 */

#include "fdlibm.h"
#include "fdlibm_intern.h"

static const double
twom54  =  5.55111512312578270212e-17; /* 0x3C900000, 0x00000000 */

double fd_scalbn (double x, int n)
{
	int  k,hx,lx;
	hx = FD_HI(x);
	lx = FD_LO(x);
    k = (hx&0x7ff00000)>>20;		/* extract exponent */
    if (k==0) {				/* 0 or subnormal x */
        if ((lx|(hx&0x7fffffff))==0) return x; /* +-0 */
	x *= two54;
	hx = FD_HI(x);
	k = ((hx&0x7ff00000)>>20) - 54;
        if (n< -50000) return gM(tiny,x); 	/*underflow*/
	}
    if (k==0x7ff) return gA(x,x);		/* NaN or Inf */
    k = k+n;
    if (k >  0x7fe) return gM(huge,fd_copysign(huge,x)); /* overflow  */
    if (k > 0) 				/* normal result */
	{FD_HI(x) = (hx&0x800fffff)|(k<<20); return x;}
    if (k <= -54)
        if (n > 50000) 	/* in case integer overflow in n+k */
	return gM(huge,fd_copysign(huge,x));	/*overflow*/
	else return gM(tiny,fd_copysign(tiny,x)); 	/*underflow*/
    k += 54;				/* subnormal result */
    FD_HI(x) = (hx&0x800fffff)|(k<<20);
    return gM(x,twom54);
}
