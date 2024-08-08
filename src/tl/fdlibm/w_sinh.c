
/* @(#)w_sinh.c 1.3 95/01/18 */
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
 * wrapper fd_sinh(x)
 */

#include "fdlibm.h"
#include "fdlibm_intern.h"

static const double shuge = 1.0e307;

double fd_sinh(double x)		/* wrapper fd_sinh */
{
	double t,w,h;
	int ix,jx;
	unsigned lx;

    /* High word of |x|. */
	jx = FD_HI(x);
	ix = jx&0x7fffffff;

    /* x is INF or NaN */
	if(ix>=0x7ff00000) return x+x;

	h = 0.5;
	if (jx<0) h = -h;
    /* |x| in [0,22], return sign(x)*0.5*(E+E/(E+1))) */
	if (ix < 0x40360000) {		/* |x|<22 */
	    if (ix<0x3e300000) 		/* |x|<2**-28 */
		if(gA(shuge,x) > one) return x;/* fd_sinh(tiny) = tiny with inexact */
	    t = fd_expm1(fd_fabs(x));
	    if(ix<0x3ff00000) return gM(h, gS(gM(2.0,t), gD(gM(t,t),gA(t,one))));
	    return gM(h,gA(t, gD(t,gA(t,one))));
	}

    /* |x| in [22, fd_log(maxdouble)] return 0.5*fd_exp(|x|) */
	if (ix < 0x40862E42)  return gM(h, fd_exp(fd_fabs(x)));

    /* |x| in [fd_log(maxdouble), overflowthresold] */
	lx = *( (((*(unsigned*)&one)>>29)) + (unsigned*)&x);
	if (ix<0x408633CE || (ix==0x408633ce)&&(lx<=(unsigned)0x8fb9f87d)) {
	    w = fd_exp(gM(0.5,fd_fabs(x)));
	    t = gM(h,w);
	    return gM(t,w);
	}

    /* |x| > overflowthresold, fd_sinh(x) overflow */
	return gM(x,shuge);
}
