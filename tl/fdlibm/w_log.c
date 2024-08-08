
/* @(#)w_log.c 1.3 95/01/18 */
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
 * wrapper fd_log(x)
 */

#include "fdlibm.h"
#include "fdlibm_intern.h"

static const double
Lg1 = 6.666666666666735130e-01,  /* 3FE55555 55555593 */
Lg2 = 3.999999999940941908e-01,  /* 3FD99999 9997FA04 */
Lg3 = 2.857142874366239149e-01,  /* 3FD24924 94229359 */
Lg4 = 2.222219843214978396e-01,  /* 3FCC71C5 1D8E78AF */
Lg5 = 1.818357216161805012e-01,  /* 3FC74664 96CB03DE */
Lg6 = 1.531383769920937332e-01,  /* 3FC39A09 D078C69F */
Lg7 = 1.479819860511658591e-01;  /* 3FC2F112 DF3E5244 */

double fd_log(double x)		/* wrapper fd_log */
{
	double hfsq,f,s,z,r,w,t1,t2,dk;
	int k,hx,i,j;
	unsigned lx;

	hx = FD_HI(x);		/* high word of x */
	lx = FD_LO(x);		/* low  word of x */

	k=0;
	if (hx < 0x00100000) {			/* x < 2**-1022  */
	    if (((hx&0x7fffffff)|lx)==0)
		return gD(-two54,zero);		/* fd_log(+-0)=-inf */
	    if (hx<0) return gD(gS(x,x),zero);	/* fd_log(-#) = NaN */
	    k -= 54; x = gM(x, two54); /* subnormal number, scale up x */
	    hx = FD_HI(x);		/* high word of x */
	}
	if (hx >= 0x7ff00000) return gA(x,x);
	k += (hx>>20)-1023;
	hx &= 0x000fffff;
	i = (hx+0x95f64)&0x100000;
	FD_HI(x) = hx|(i^0x3ff00000);	/* normalize x or x/2 */
	k += (i>>20);
	f = gS(x,1.0);
	if((0x000fffff&(2+hx))<3) {	/* |f| < 2**-20 */
	    if(f==zero) if(k==0) return zero;  else {dk=(double)k;
				 return gA(gM(dk,ln2_hi), gM(dk,ln2_lo));}
	    r = gM(gM(f,f), gS(0.5, gM(0.33333333333333333,f)));
	    if(k==0) return gS(f,r); else {dk=(double)k;
	    	     return gS(gM(dk,ln2_hi), gS(gS(r,gM(dk,ln2_lo)),f));}
	}
 	s = gD(f,gA(2.0,f));
	dk = (double)k;
	z = gM(s,s);
	i = hx-0x6147a;
	w = gM(z,z);
	j = 0x6b851-hx;
	t1= gM(w,gA(Lg2, gM(w,gA(Lg4, gM(w,Lg6)))));
	t2= gM(z,gA(Lg1, gM(w,gA(Lg3, gM(w,gA(Lg5, gM(w,Lg7)))))));
	i |= j;
	r = gA(t2,t1);
	if(i>0) {
	    hfsq = gM(gM(0.5,f),f);
	    if(k==0) return gS(f,gS(hfsq,gM(s,gA(hfsq,r)))); else
		     return gS(gM(dk,ln2_hi), gS(gS(hfsq, gA(gM(s,gA(hfsq,r)), gM(dk,ln2_lo))), f));
	} else {
	    if(k==0) return gS(f, gM(s, gS(f, r))); else
		     return gS(gM(dk,ln2_hi), gS(gS(gM(s,gS(f,r)), gM(dk,ln2_lo)), f));
	}
}
