
/* @(#)w_atanh.c 1.3 95/01/18 */
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
 * wrapper fd_atanh(x)
 */

#include "fdlibm.h"
#include "fdlibm_intern.h"

double fd_atanh(double x)		/* wrapper fd_atanh */
{
	double t;
	int hx,ix;
	unsigned lx;
	hx = FD_HI(x);		/* high word */
	lx = FD_LO(x);		/* low word */
	ix = hx&0x7fffffff;
	if ((ix|((lx|((0-lx)))>>31))>0x3ff00000) /* |x|>1 */
	    return gD(gS(x,x), gS(x,x));
	if(ix==0x3ff00000)
	    return gD(x,zero);
	if(ix<0x3e300000 && gA(huge,x)>zero) return x;	/* x<2**-28 */
	FD_HI(x) = ix;		/* x <- |x| */
	if(ix<0x3fe00000) {		/* x < 0.5 */
	    t = gA(x,x);
	    t = gM(0.5, fd_log1p(gA(t, gD(gM(t,x), gS(one,x)))));
	} else
	    t = gM(0.5, fd_log1p(gD(gA(x,x), gS(one,x))));
	if(hx>=0) return t; else return -t;
}
