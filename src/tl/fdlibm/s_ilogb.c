
/* @(#)s_ilogb.c 1.3 95/01/18 */
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

/* fd_ilogb(double x)
 * return the binary exponent of non-zero x
 * fd_ilogb(0) = 0x80000001
 * fd_ilogb(inf/NaN) = 0x7fffffff (no signal is raised)
 */

#include "fdlibm.h"
#include "fdlibm_intern.h"

int fd_ilogb(double x)
{
	int hx,lx,ix;

	hx  = (FD_HI(x))&0x7fffffff;	/* high word of x */
	if(hx<0x00100000) {
	    lx = FD_LO(x);
	    if((hx|lx)==0)
		return 0x80000001;	/* fd_ilogb(0) = 0x80000001 */
	    else			/* subnormal x */
		if(hx==0) {
		    for (ix = -1043; lx>0; lx<<=1) ix -=1;
		} else {
		    for (ix = -1022,hx<<=11; hx>0; hx<<=1) ix -=1;
		}
	    return ix;
	}
	else if (hx<0x7ff00000) return (hx>>20)-1023;
	else return 0x7fffffff;
}
