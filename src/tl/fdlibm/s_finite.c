
/* @(#)s_finite.c 1.3 95/01/18 */
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
 * fd_finite(x) returns 1 is x is fd_finite, else 0;
 * no branching!
 */

#include "fdlibm.h"
#include "fdlibm_intern.h"

int fd_finite(double x)
{
	int hx;
	hx = FD_HI(x);
	return  (unsigned)((hx&0x7fffffff)-0x7ff00000)>>31;
}
