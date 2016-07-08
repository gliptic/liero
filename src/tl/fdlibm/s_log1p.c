
/* @(#)s_log1p.c 1.3 95/01/18 */
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

/* double fd_log1p(double x)
 *
 * Method :
 *   1. Argument Reduction: find k and f such that
 *			1+x = 2^k * (1+f),
 *	   where  fd_sqrt(2)/2 < 1+f < fd_sqrt(2) .
 *
 *      Note. If k=0, then f=x is exact. However, if k!=0, then f
 *	may not be representable exactly. In that case, a correction
 *	term is need. Let u=1+x rounded. Let c = (1+x)-u, then
 *	fd_log(1+x) - fd_log(u) ~ c/u. Thus, we proceed to compute fd_log(u),
 *	and add back the correction term c/u.
 *	(Note: when x > 2**53, one can simply return fd_log(x))
 *
 *   2. Approximation of fd_log1p(f).
 *	Let s = f/(2+f) ; based on fd_log(1+f) = fd_log(1+s) - fd_log(1-s)
 *		 = 2s + 2/3 s**3 + 2/5 s**5 + .....,
 *	     	 = 2s + s*r
 *      We use a special Reme algorithm on [0,0.1716] to generate
 * 	a polynomial of degree 14 to approximate r The maximum error
 *	of this polynomial approximation is bounded by 2**-58.45. In
 *	other words,
 *		        2      4      6      8      10      12      14
 *	    R(z) ~ Lp1*s +Lp2*s +Lp3*s +Lp4*s +Lp5*s  +Lp6*s  +Lp7*s
 *  	(the values of Lp1 to Lp7 are listed in the program)
 *	and
 *	    |      2          14          |     -58.45
 *	    | Lp1*s +...+Lp7*s    -  R(z) | <= 2
 *	    |                             |
 *	Note that 2s = f - s*f = f - hfsq + s*hfsq, where hfsq = f*f/2.
 *	In order to guarantee error in fd_log below 1ulp, we compute fd_log
 *	by
 *		fd_log1p(f) = f - (hfsq - s*(hfsq+r)).
 *
 *	3. Finally, fd_log1p(x) = k*ln2 + fd_log1p(f).
 *		 	     = k*ln2_hi+(f-(hfsq-(s*(hfsq+r)+k*ln2_lo)))
 *	   Here ln2 is split into two floating point number:
 *			ln2_hi + ln2_lo,
 *	   where n*ln2_hi is always exact for |n| < 2000.
 *
 * Special cases:
 *	fd_log1p(x) is NaN with signal if x < -1 (including -INF) ;
 *	fd_log1p(+INF) is +INF; fd_log1p(-1) is -INF with signal;
 *	fd_log1p(NaN) is that NaN with no signal.
 *
 * Accuracy:
 *	according to an error analysis, the error is always less than
 *	1 ulp (unit in the last place).
 *
 * Constants:
 * The hexadecimal values are the intended ones for the following
 * constants. The decimal values may be used, provided that the
 * compiler will convert from decimal to binary accurately enough
 * to produce the hexadecimal values shown.
 *
 * Note: Assuming fd_log() return accurate answer, the following
 * 	 algorithm can be used to compute fd_log1p(x) to within a few ULP:
 *
 *		u = 1+x;
 *		if(u==1.0) return x ; else
 *			   return fd_log(u)*(x/(u-1.0));
 *
 *	 See HP-15C Advanced Functions Handbook, p.193.
 */

#include "fdlibm.h"
#include "fdlibm_intern.h"

static const double
Lp1 = 6.666666666666735130e-01,  /* 3FE55555 55555593 */
Lp2 = 3.999999999940941908e-01,  /* 3FD99999 9997FA04 */
Lp3 = 2.857142874366239149e-01,  /* 3FD24924 94229359 */
Lp4 = 2.222219843214978396e-01,  /* 3FCC71C5 1D8E78AF */
Lp5 = 1.818357216161805012e-01,  /* 3FC74664 96CB03DE */
Lp6 = 1.531383769920937332e-01,  /* 3FC39A09 D078C69F */
Lp7 = 1.479819860511658591e-01;  /* 3FC2F112 DF3E5244 */

double fd_log1p(double x)
{
	double hfsq,f,c,s,z,r,u;
	int k,hx,hu,ax;

	hx = FD_HI(x);		/* high word of x */
	ax = hx&0x7fffffff;

	k = 1;
	if (hx < 0x3FDA827A) {			/* x < 0.41422  */
	    if(ax>=0x3ff00000) {		/* x <= -1.0 */
		if(x==-1.0) return -two54/zero; /* fd_log1p(-1)=+inf */
		else return gD(gS(x,x),gS(x,x));	/* fd_log1p(x<-1)=NaN */
	    }
	    if(ax<0x3e200000) {			/* |x| < 2**-29 */
		if(gA(two54, x) > zero			/* raise inexact */
	            &&ax<0x3c900000) 		/* |x| < 2**-54 */
		    return x;
		else
		    return gS(x, gM(gM(x,x),0.5));
	    }
	    if(hx>0||hx<=((int)0xbfd2bec3)) {
		k=0;f=x;hu=1;}	/* -0.2929<x<0.41422 */
	}
	if (hx >= 0x7ff00000) return gA(x,x);
	if(k!=0) {
	    if(hx<0x43400000) {
		u  = gA(1.0, x);
	        hu = FD_HI(u);		/* high word of u */
	        k  = (hu>>20)-1023;
	        c  = (k>0)? gS(1.0,gS(u,x)) : gS(x,gS(u,1.0));/* correction term */
		c = gD(c, u);
	    } else {
		u  = x;
	        hu = FD_HI(u);		/* high word of u */
	        k  = (hu>>20)-1023;
		c  = 0;
	    }
	    hu &= 0x000fffff;
	    if(hu<0x6a09e) {
	        FD_HI(u) = hu|0x3ff00000;	/* normalize u */
	    } else {
	        k += 1;
	        FD_HI(u) = hu|0x3fe00000;	/* normalize u/2 */
	        hu = (0x00100000-hu)>>2;
	    }
	    f = gS(u,1.0);
	}
	hfsq = gM(gM(0.5,f),f);
	if(hu==0) {	/* |f| < 2**-20 */
	    if(f==zero) if(k==0) return zero;
			else {c = gA(c,gM(k,ln2_lo)); return gA(gM(k,ln2_hi), c);}
	    r = gM(hfsq,gS(1.0, gM(0.66666666666666666,f)));
	    if(k==0) return gS(f,r); else
	    	     return gS(gM(k,ln2_hi), gS(gS(r, gA(gM(k,ln2_lo), c)), f));
	}
 	s = gD(f,gA(2.0, f));
	z = gM(s,s);
	r = gM(z,gA(Lp1, gM(z,gA(Lp2, gM(z,gA(Lp3, gM(z,gA(Lp4, gM(z,gA(Lp5, gM(z,gA(Lp6, gM(z,Lp7)))))))))))));
	if(k==0) return gS(f, gS(hfsq, gM(s,gA(hfsq,r)))); else
		 return gS(gM(k,ln2_hi), gS(gS(hfsq, gA(gM(s,gA(hfsq,r)), gA(gM(k,ln2_lo), c))), f));
}
