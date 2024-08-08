
/* @(#)k_cos.c 1.3 95/01/18 */
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
 * _kernel_cos( x,  y )
 * kernel fd_cos function on [-pi/4, pi/4], pi/4 ~ 0.785398164
 * Input x is assumed to be bounded by ~pi/4 in magnitude.
 * Input y is the tail of x.
 *
 * Algorithm
 *	1. Since fd_cos(-x) = fd_cos(x), we need only to consider positive x.
 *	2. if x < 2^-27 (hx<0x3e400000 0), return 1 with inexact if x!=0.
 *	3. fd_cos(x) is approximated by a polynomial of degree 14 on
 *	   [0,pi/4]
 *		  	                 4            14
 *	   	fd_cos(x) ~ 1 - x*x/2 + C1*x + ... + C6*x
 *	   where the remez error is
 *
 * 	|              2     4     6     8     10    12     14 |     -58
 * 	|fd_cos(x)-(1-.5*x +C1*x +C2*x +C3*x +C4*x +C5*x  +C6*x  )| <= 2
 * 	|    					               |
 *
 * 	               4     6     8     10    12     14
 *	4. let r = C1*x +C2*x +C3*x +C4*x +C5*x  +C6*x  , then
 *	       fd_cos(x) = 1 - x*x/2 + r
 *	   since fd_cos(x+y) ~ fd_cos(x) - fd_sin(x)*y
 *			  ~ fd_cos(x) - x*y,
 *	   a correction term is necessary in fd_cos(x) and hence
 *		fd_cos(x+y) = 1 - (x*x/2 - (r - x*y))
 *	   For better accuracy when x > 0.3, let qx = |x|/4 with
 *	   the last 32 bits mask off, and if x > 0.78125, let qx = 0.28125.
 *	   Then
 *		fd_cos(x+y) = (1-qx) - ((x*x/2-qx) - (r-x*y)).
 *	   Note that 1-qx and (x*x/2-qx) is EXACT here, and the
 *	   magnitude of the latter is at least a quarter of x*x/2,
 *	   thus, reducing the rounding error in the subtraction.
 */

#include "fdlibm.h"
#include "fdlibm_intern.h"

static const double
C1  =  4.16666666666666019037e-02, /* 0x3FA55555, 0x5555554C */
C2  = -1.38888888888741095749e-03, /* 0xBF56C16C, 0x16C15177 */
C3  =  2.48015872894767294178e-05, /* 0x3EFA01A0, 0x19CB1590 */
C4  = -2.75573143513906633035e-07, /* 0xBE927E4F, 0x809C52AD */
C5  =  2.08757232129817482790e-09, /* 0x3E21EE9E, 0xBDB4B1C4 */
C6  = -1.13596475577881948265e-11; /* 0xBDA8FAE9, 0xBE8838D4 */

FDLIBM_INTERNAL double _kernel_cos(double x, double y)
{
	double a,hz,z,r,qx;
	int ix;
	ix = FD_HI(x)&0x7fffffff;	/* ix = |x|'s high word*/
	if(ix<0x3e400000) {			/* if x < 2**27 */
	    if(((int)x)==0) return one;		/* generate inexact */
	}
	z = gM(x,x);
	r = gM(z,gA(C1, gM(z,gA(C2, gM(z,gA(C3, gM(z,gA(C4, gM(z,gA(C5, gM(z,C6)))))))))));
	if(ix < 0x3FD33333) 			/* if |x| < 0.3 */
	    return gS(one, gS(gM(0.5,z), gS(gM(z,r), gM(x,y))));
	else {
	    if(ix > 0x3fe90000) {		/* x > 0.78125 */
		qx = 0.28125;
	    } else {
	        FD_HI(qx) = ix-0x00200000;	/* x/4 */
	        FD_LO(qx) = 0;
	    }
	    hz = gS(gM(0.5,z), qx);
	    a  = gS(one,qx);
	    return gS(a, gS(hz, gS(gM(z,r), gM(x,y))));
	}
}
