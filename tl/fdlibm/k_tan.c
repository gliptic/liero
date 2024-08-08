
/* @(#)k_tan.c 1.3 95/01/18 */
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

/* _kernel_tan( x, y, k )
 * kernel fd_tan function on [-pi/4, pi/4], pi/4 ~ 0.7854
 * Input x is assumed to be bounded by ~pi/4 in magnitude.
 * Input y is the tail of x.
 * Input k indicates whether fd_tan (if k=1) or
 * -1/fd_tan (if k= -1) is returned.
 *
 * Algorithm
 *	1. Since fd_tan(-x) = -fd_tan(x), we need only to consider positive x.
 *	2. if x < 2^-28 (hx<0x3e300000 0), return x with inexact if x!=0.
 *	3. fd_tan(x) is approximated by a odd polynomial of degree 27 on
 *	   [0,0.67434]
 *		  	         3             27
 *	   	fd_tan(x) ~ x + T1*x + ... + T13*x
 *	   where
 *
 * 	        |fd_tan(x)         2     4            26   |     -59.2
 * 	        |----- - (1+T1*x +T2*x +.... +T13*x    )| <= 2
 * 	        |  x 					|
 *
 *	   Note: fd_tan(x+y) = fd_tan(x) + fd_tan'(x)*y
 *		          ~ fd_tan(x) + (1+x*x)*y
 *	   Therefore, for better accuracy in computing fd_tan(x+y), let
 *		     3      2      2       2       2
 *		r = x *(T2+x *(T3+x *(...+x *(T12+x *T13))))
 *	   then
 *		 		    3    2
 *		fd_tan(x+y) = x + (T1*x + (x *(r+y)+y))
 *
 *      4. For x in [0.67434,pi/4],  let y = pi/4 - x, then
 *		fd_tan(x) = fd_tan(pi/4-y) = (1-fd_tan(y))/(1+fd_tan(y))
 *		       = 1 - 2*(fd_tan(y) - (fd_tan(y)^2)/(1+fd_tan(y)))
 */

#include "fdlibm.h"
#include "fdlibm_intern.h"

static const double
pio4  =  7.85398163397448278999e-01, /* 0x3FE921FB, 0x54442D18 */
pio4lo=  3.06161699786838301793e-17, /* 0x3C81A626, 0x33145C07 */
T[] =  {
  3.33333333333334091986e-01, /* 0x3FD55555, 0x55555563 */
  1.33333333333201242699e-01, /* 0x3FC11111, 0x1110FE7A */
  5.39682539762260521377e-02, /* 0x3FABA1BA, 0x1BB341FE */
  2.18694882948595424599e-02, /* 0x3F9664F4, 0x8406D637 */
  8.86323982359930005737e-03, /* 0x3F8226E3, 0xE96E8493 */
  3.59207910759131235356e-03, /* 0x3F6D6D22, 0xC9560328 */
  1.45620945432529025516e-03, /* 0x3F57DBC8, 0xFEE08315 */
  5.88041240820264096874e-04, /* 0x3F4344D8, 0xF2F26501 */
  2.46463134818469906812e-04, /* 0x3F3026F7, 0x1A8D1068 */
  7.81794442939557092300e-05, /* 0x3F147E88, 0xA03792A6 */
  7.14072491382608190305e-05, /* 0x3F12B80F, 0x32F0A7E9 */
 -1.85586374855275456654e-05, /* 0xBEF375CB, 0xDB605373 */
  2.59073051863633712884e-05, /* 0x3EFB2A70, 0x74BF7AD4 */
};

FDLIBM_INTERNAL double _kernel_tan(double x, double y, int iy)
{
	double z,r,v,w,s;
	int ix,hx;
	hx = FD_HI(x);	/* high word of x */
	ix = hx&0x7fffffff;	/* high word of |x| */
	if(ix<0x3e300000)			/* x < 2**-28 */
	    {if((int)x==0) {			/* generate inexact */
		if(((ix|FD_LO(x))|(iy+1))==0) return gD(one, fd_fabs(x));
		else return (iy==1)? x: gD(-one,x);
	    }
	    }
	if(ix>=0x3FE59428) { 			/* |x|>=0.6744 */
	    if(hx<0) {x = -x; y = -y;}
	    z = gS(pio4,x);
	    w = gS(pio4lo,y);
	    x = gA(z,w); y = 0.0;
	}
	z	=  gM(x,x);
	w 	=  gM(z,z);
    /* Break x^5*(T[1]+x^2*T[2]+...) into
     *	  x^5(T[1]+x^4*T[3]+...+x^20*T[11]) +
     *	  x^5(x^2*(T[2]+x^4*T[4]+...+x^22*[T12]))
     */
	r = gA(T[1], gM(w,gA(T[3], gM(w,gA(T[5], gM(w,gA(T[7], gM(w,gA(T[9],  gM(w,T[11]))))))))));
	v = gM(z, gA(T[2], gM(w,gA(T[4], gM(w,gA(T[6], gM(w,gA(T[8], gM(w,gA(T[10], gM(w,T[12])))))))))));
	s = gM(z,x);
	r = gA(y, gM(z, gA(gM(s,gA(r,v)), y)));
	r = gA(r, gM(T[0],s));
	w = gA(x,r);
	if(ix>=0x3FE59428) {
	    v = (double)iy;
	    return (double)(1-((hx>>30)&2))*gS(v, gM(2.0,gS(x, gS(gD(gM(w,w),gA(w, v)), r))));
	}
	if(iy==1) return w;
	else {		/* if allow error up to 2 ulp,
			   simply return -1.0/(x+r) here */
     /*  compute -1.0/(x+r) accurately */
	    double a,t;
	    z  = w;
	    FD_LO(z) = 0;
	    v = gS(r, gS(z, x)); 	/* z+v = r+x */
	    t = a = gD(-1.0,w);	/* a = -1.0/w */
	    FD_LO(t) = 0;
	    s = gA(1.0, gM(t,z));
	    return gA(t, gM(a,gA(s, gM(t,v))));
	}
}
