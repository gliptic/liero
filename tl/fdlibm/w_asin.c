
/* @(#)w_asin.c 1.3 95/01/18 */
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

/*
 * wrapper fd_asin(x)
 */


#include "fdlibm.h"
#include "fdlibm_intern.h"

double fd_asin(double x)		/* wrapper fd_asin */
{
	double t,w,p,q,c,r,s;
	int hx,ix;
	hx = FD_HI(x);
	ix = hx&0x7fffffff;
	if(ix>= 0x3ff00000) {		/* |x|>= 1 */
		if(((ix-0x3ff00000)|FD_LO(x))==0)
			/* fd_asin(1)=+-pi/2 with inexact */
		return gA(gM(x,pio2_hi), gM(x,pio2_lo));
		return gD(gS(x,x),gS(x,x));		/* fd_asin(|x|>1) is NaN */
	} else if (ix<0x3fe00000) {	/* |x|<0.5 */
		if(ix<0x3e400000) {		/* if |x| < 2**-27 */
		if(gA(huge,x) > one) return x;/* return x with inexact if x!=0*/
		} else
		t = gM(x,x);
		p = gM(t,gA(pS0, gM(t,gA(pS1, gM(t,gA(pS2, gM(t,gA(pS3, gM(t,gA(pS4, gM(t,pS5)))))))))));
		q = gA(one, gM(t,gA(qS1, gM(t,gA(qS2, gM(t,gA(qS3, gM(t,qS4))))))));
		w = gD(p, q);
		return gA(x, gM(x,w));
	}
	/* 1> |x|>= 0.5 */
	w = gS(one,fd_fabs(x));
	t = gM(w,0.5);
	p = gM(t,gA(pS0, gM(t,gA(pS1, gM(t,gA(pS2, gM(t,gA(pS3, gM(t,gA(pS4, gM(t,pS5)))))))))));
	q = gA(one, gM(t,gA(qS1, gM(t,gA(qS2, gM(t,gA(qS3, gM(t,qS4))))))));
	s = gSqrt(t);
	if(ix>=0x3FEF3333) { 	/* if |x| > 0.975 */
		w = gD(p,q);
		t = gS(pio2_hi, gS(gM(2.0, gA(s, gM(s,w))), pio2_lo));
	} else {
		w  = s;
		FD_LO(w) = 0;
		c  = gD(gS(t, gM(w,w)), gA(s,w));
		r  = gD(p,q);
		p  = gS(gM(gM(2.0,s),r), gS(pio2_lo,gM(2.0,c)));
		q  = gS(pio4_hi, gM(2.0,w));
		t  = gS(pio4_hi, gS(p,q));
	}
	if(hx>0) return t; else return -t;
}
