
/* @(#)w_scalb.c 1.3 95/01/18 */
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
 * wrapper fd_scalb(double x, double fn) is provide for
 * passing various standard test suite. One
 * should use fd_scalbn() instead.
 */

#include "fdlibm.h"
#include "fdlibm_intern.h"

#include <errno.h>

#ifdef _SCALB_INT
	double fd_scalb(double x, int fn)		/* wrapper fd_scalb */
#else
	double fd_scalb(double x, double fn)	/* wrapper fd_scalb */
#endif
{
	return fd_scalbn(x,fn);
}
