
/* @(#)s_significand.c 1.3 95/01/18 */
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
 * fd_significand(x) computes just
 * 	fd_scalb(x, (double) -fd_ilogb(x)),
 * for exercising the fraction-part(F) IEEE 754-1985 test vector.
 */

#include "fdlibm.h"
#include "fdlibm_intern.h"

double fd_significand(double x)
{
	return fd_scalb(x,(double) -fd_ilogb(x));
}
