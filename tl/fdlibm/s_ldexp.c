
/* @(#)s_ldexp.c 1.3 95/01/18 */
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

#include "fdlibm.h"
#include "fdlibm_intern.h"
#include <errno.h>

double fd_ldexp(double value, int fd_exp)
{
	if(!fd_finite(value)||value==0.0) return value;
	value = fd_scalbn(value,fd_exp);
	if(!fd_finite(value)||value==0.0) errno = ERANGE;
	return value;
}
