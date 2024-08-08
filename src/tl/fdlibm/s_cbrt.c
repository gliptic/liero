
/* @(#)s_cbrt.c 1.3 95/01/18 */
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

#include "fdlibm.h"
#include "fdlibm_intern.h"

/* fd_cbrt(x)
 * Return cube root of x
 */

static const unsigned
	B1 = 715094163, /* B1 = (682-0.03306235651)*2**20 */
	B2 = 696219795; /* B2 = (664-0.03306235651)*2**20 */

static const double
C =  5.42857142857142815906e-01, /* 19/35     = 0x3FE15F15, 0xF15F15F1 */
D = -7.05306122448979611050e-01, /* -864/1225 = 0xBFE691DE, 0x2532C834 */
E =  1.41428571428571436819e+00, /* 99/70     = 0x3FF6A0EA, 0x0EA0EA0F */
F =  1.60714285714285720630e+00, /* 45/28     = 0x3FF9B6DB, 0x6DB6DB6E */
G =  3.57142857142857150787e-01; /* 5/14      = 0x3FD6DB6D, 0xB6DB6DB7 */

double fd_cbrt(double x)
{
	int	hx;
	double r,s,t=0.0,w;
	unsigned sign;


	hx = FD_HI(x);		/* high word of x */
	sign=hx&0x80000000; 		/* sign= sign(x) */
	hx  ^=sign;
	if(hx>=0x7ff00000) return gA(x,x); /* fd_cbrt(NaN,INF) is itself */
	if((hx|FD_LO(x))==0)
	    return(x);		/* fd_cbrt(0) is itself */

	FD_HI(x) = hx;	/* x <- |x| */
    /* rough fd_cbrt to 5 bits */
	if(hx<0x00100000) 		/* subnormal number */
	  {FD_HI(t)=0x43500000; 		/* set t= 2**54 */
	   t = gM(t,x); FD_HI(t)=FD_HI(t)/3+B2;
	  }
	else
	  FD_HI(t)=hx/3+B1;


    /* new fd_cbrt to 23 bits, may be implemented in single precision */
	r = gD(gM(t,t),x);
	s = gA(C, gM(r,t));
	t = gM(t, gA(G, gD(F,gA(gA(s, E), gD(D,s)))));

    /* chopped to 20 bits and make it larger than fd_cbrt(x) */
	FD_LO(t)=0; FD_HI(t)+=0x00000001;


    /* one step newton iteration to 53 bits with error less than 0.667 ulps */
	s = gM(t, t);		/* t*t is exact */
	r = gD(x, s);
	w = gA(t, t);
	r = gD(gS(r, t), gA(w, r));	/* r-s is exact */
	t = gA(t, gM(t,r));

    /* retore the sign bit */
	FD_HI(t) |= sign;
	return(t);
}
