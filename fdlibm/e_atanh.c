
/* @(#)e_atanh.c 1.3 95/01/18 */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 *
 */

/* _ieee754_atanh(x)
 * Method :
 *    1.Reduced x to positive by fd_atanh(-x) = -fd_atanh(x)
 *    2.For x>=0.5
 *                  1              2x                          x
 *	fd_atanh(x) = --- * fd_log(1 + -------) = 0.5 * fd_log1p(2 * --------)
 *                  2             1 - x                      1 - x
 *
 * 	For x<0.5
 *	fd_atanh(x) = 0.5*fd_log1p(2x+2x*x/(1-x))
 *
 * Special cases:
 *	fd_atanh(x) is NaN if |x| > 1 with signal;
 *	fd_atanh(NaN) is that NaN with no signal;
 *	fd_atanh(+-1) is +-INF with signal.
 *
 */

#include "fdlibm.h"
#include "fdlibm_intern.h"
