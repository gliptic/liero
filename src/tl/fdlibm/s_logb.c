
/* @(#)s_logb.c 1.3 95/01/18 */
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
 * double fd_logb(x)
 * IEEE 754 fd_logb. Included to pass IEEE test suite. Not recommend.
 * Use fd_ilogb instead.
 */

#include "fdlibm.h"
#include "fdlibm_intern.h"

double fd_logb(double x)
{
	int lx,ix;
	ix = (FD_HI(x))&0x7fffffff;	/* high |x| */
	lx = FD_LO(x);			/* low x */
	if((ix|lx)==0) return gD(-1.0,fd_fabs(x));
	if(ix>=0x7ff00000) return gM(x,x);
	if((ix>>=20)==0) 			/* IEEE 754 fd_logb */
		return -1022.0;
	else
		return (double) (ix-1023);
}
