
/* @(#)w_exp.c 1.3 95/01/18 */
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
 * wrapper fd_exp(x)
 */

#include "fdlibm.h"
#include "fdlibm_intern.h"

static const double
halF[2]	= {0.5,-0.5,},
twom1000= 9.33263618503218878990e-302,     /* 2**-1000=0x01700000,0*/
ln2HI[2]   ={ 6.93147180369123816490e-01,  /* 0x3fe62e42, 0xfee00000 */
	     -6.93147180369123816490e-01,},/* 0xbfe62e42, 0xfee00000 */
ln2LO[2]   ={ 1.90821492927058770002e-10,  /* 0x3dea39ef, 0x35793c76 */
	     -1.90821492927058770002e-10,},/* 0xbdea39ef, 0x35793c76 */
P1   =  1.66666666666666019037e-01, /* 0x3FC55555, 0x5555553E */
P2   = -2.77777777770155933842e-03, /* 0xBF66C16C, 0x16BEBD93 */
P3   =  6.61375632143793436117e-05, /* 0x3F11566A, 0xAF25DE2C */
P4   = -1.65339022054652515390e-06, /* 0xBEBBBD41, 0xC5D26BF1 */
P5   =  4.13813679705723846039e-08; /* 0x3E663769, 0x72BEA4D0 */

double fd_exp(double x)		/* wrapper fd_exp */
{
	double y,hi,lo,c,t;
	int k,xsb;
	unsigned hx;

	hx  = FD_HI(x);	/* high word of x */
	xsb = (hx>>31)&1;		/* sign bit of x */
	hx &= 0x7fffffff;		/* high word of |x| */

    /* filter out non-fd_finite argument */
	if(hx >= 0x40862E42) {			/* if |x|>=709.78... */
            if(hx>=0x7ff00000) {
		if(((hx&0xfffff)|FD_LO(x))!=0)
		     return gA(x,x); 		/* NaN */
		else return (xsb==0)? x:0.0;	/* fd_exp(+-inf)={inf,0} */
	    }
	    if(x > o_threshold) return gM(huge,huge); /* overflow */
	    if(x < u_threshold) return gM(twom1000,twom1000); /* underflow */
	}

    /* argument reduction */
	if(hx > 0x3fd62e42) {		/* if  |x| > 0.5 ln2 */
	    if(hx < 0x3FF0A2B2) {	/* and |x| < 1.5 ln2 */
		hi = gS(x,ln2HI[xsb]); lo=ln2LO[xsb]; k = ((1-xsb)-xsb);
	    } else {
		k  = (int)gA(gM(invln2,x), halF[xsb]);
		t  = k;
		hi = gS(x, gM(t,ln2HI[0]));	/* t*ln2HI is exact here */
		lo = gM(t,ln2LO[0]);
	    }
	    x  = gS(hi, lo);
	}
	else if(hx < 0x3e300000)  {	/* when |x|<2**-28 */
	    if(gA(huge,x)>one) return one+x;/* trigger inexact */
	}
	else k = 0;

    /* x is now in primary range */
	t  = gM(x,x);
	c  = gS(x, gM(t,gA(P1, gM(t,gA(P2, gM(t,gA(P3, gM(t,gA(P4, gM(t,P5))))))))));
	if(k==0) 	return gS(one, gS(gD(gM(x,c), gS(c,2.0)), x));
	else 		y = gS(one, gS(gS(lo, gD(gM(x,c), gS(2.0,c))), hi));
	if(k >= -1021) {
	    FD_HI(y) += (k<<20);	/* add k to y's exponent */
	    return y;
	} else {
	    FD_HI(y) += ((k+1000)<<20);/* add k to y's exponent */
	    return gM(y,twom1000);
	}
}
