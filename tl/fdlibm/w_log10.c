
/* @(#)w_log10.c 1.3 95/01/18 */
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
 * wrapper fd_log10(X)
 */

#include "fdlibm.h"
#include "fdlibm_intern.h"

static const double
ivln10     =  4.34294481903251816668e-01, /* 0x3FDBCB7B, 0x1526E50E */
log10_2hi  =  3.01029995663611771306e-01, /* 0x3FD34413, 0x509F6000 */
log10_2lo  =  3.69423907715893078616e-13; /* 0x3D59FEF3, 0x11F12B36 */

double fd_log10(double x)		/* wrapper fd_log10 */
{
	double y,z;
	int i,k,hx;
	unsigned lx;

	hx = FD_HI(x);	/* high word of x */
	lx = FD_LO(x);	/* low word of x */

    k=0;
    if (hx < 0x00100000) {                  /* x < 2**-1022  */
        if (((hx&0x7fffffff)|lx)==0)
            return gD(-two54,zero);             /* fd_log(+-0)=-inf */
        if (hx<0) return gD(gS(x,x),zero);        /* fd_log(-#) = NaN */
        k -= 54; x = gM(x,two54); /* subnormal number, scale up x */
        hx = FD_HI(x);                /* high word of x */
    }
	if (hx >= 0x7ff00000) return gA(x,x);
	k += (hx>>20)-1023;
	i  = ((unsigned)k&0x80000000)>>31;
        hx = (hx&0x000fffff)|((0x3ff-i)<<20);
        y  = (double)(k+i);
        FD_HI(x) = hx;
	z  = gA(gM(y,log10_2lo), gM(ivln10, fd_log(x)));
	return  gA(z, gM(y,log10_2hi));
}
