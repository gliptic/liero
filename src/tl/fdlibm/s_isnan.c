
/* @(#)s_isnan.c 1.3 95/01/18 */
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
 * fd_isnan(x) returns 1 is x is nan, else 0;
 * no branching!
 */

#include "fdlibm.h"
#include "fdlibm_intern.h"

int fd_isnan(double x)
{
	int hx,lx;
	hx = (FD_HI(x)&0x7fffffff);
	lx = FD_LO(x);
	hx |= (unsigned)(lx|(-lx))>>31;
	hx = 0x7ff00000 - hx;
	return ((unsigned)(hx))>>31;
}
