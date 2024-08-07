
/* @(#)s_copysign.c 1.3 95/01/18 */
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
 * fd_copysign(double x, double y)
 * fd_copysign(x,y) returns a value with the magnitude of x and
 * with the sign bit of y.
 */

#include "fdlibm.h"
#include "fdlibm_intern.h"

double fd_copysign(double x, double y)
{
	FD_HI(x) = (FD_HI(x)&0x7fffffff)|(FD_HI(y)&0x80000000);
    return x;
}
