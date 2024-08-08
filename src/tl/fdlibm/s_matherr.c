
/* @(#)s_matherr.c 1.3 95/01/18 */
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

int fd_matherr(struct fd_exception *x)
{
	int n=0;
	if(x->arg1!=x->arg1) return 0;
	return n;
}
