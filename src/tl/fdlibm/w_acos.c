
/* @(#)w_acos.c 1.3 95/01/18 */
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
 * wrap_acos(x)
 */

#include "fdlibm.h"
#include "fdlibm_intern.h"


double fd_acos(double x)		/* wrapper fd_acos */
{
	double z,p,q,r,w,s,c,df;
	int hx,ix;
	hx = FD_HI(x);
	ix = hx&0x7fffffff;
	if(ix>=0x3ff00000) {	/* |x| >= 1 */
		if(((ix-0x3ff00000)|FD_LO(x))==0) {	/* |x|==1 */
		if(hx>0) return 0.0;		/* fd_acos(1) = 0  */
		else return gA(pi, gM(2.0, pio2_lo));	/* fd_acos(-1)= pi */
		}
		return gD(gS(x,x), gS(x,x));		/* fd_acos(|x|>1) is NaN */
	}
	if(ix<0x3fe00000) {	/* |x| < 0.5 */
		if(ix<=0x3c600000) return gA(pio2_hi, pio2_lo);/*if|x|<2**-57*/
		z = gM(x, x);
		p = gM(z,gA(pS0,gM(z,gA(pS1,gM(z,gA(pS2,gM(z,gA(pS3,gM(z,gA(pS4,gM(z,pS5)))))))))));
		q = gA(one,gM(z,gA(qS1,gM(z,gA(qS2,gM(z,gA(qS3,gM(z,qS4))))))));
		r = gD(p,q);
		return gS(pio2_hi, gS(x, gS(pio2_lo, gM(x,r))));
	} else  if (hx<0) {		/* x < -0.5 */
		z = gM(gA(one, x), 0.5);
		p = gM(z, gA(pS0,gM(z,gA(pS1,gM(z,gA(pS2,gM(z,gA(pS3,gM(z,gA(pS4,gM(z,pS5)))))))))));
		q = gA(one, gM(z,gA(qS1,gM(z,gA(qS2,gM(z,gA(qS3,gM(z,qS4))))))));
		s = gSqrt(z);
		r = gD(p, q);
		w = gS(gM(r, s), pio2_lo);
		return gS(pi, gM(2.0, gA(s,w)));
	} else {			/* x > 0.5 */
		z = gM(gS(one, x), 0.5);
		s = gSqrt(z);
		df = s;
		FD_LO(df) = 0;
		c  = gD(gS(z, gM(df,df)), gA(s,df));
		p = gM(z,gA(pS0,gM(z,gA(pS1,gM(z,gA(pS2,gM(z,gA(pS3,gM(z,gA(pS4,gM(z,pS5)))))))))));
		q = gA(one, gM(z,gA(qS1,gM(z,gA(qS2,gM(z,gA(qS3,gM(z,qS4))))))));
		r = gD(p, q);
		w = gA(gM(r, s), c);
		return gM(2.0, gA(df, w));
	}
}
