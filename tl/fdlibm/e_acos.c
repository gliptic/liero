
/* @(#)e_acos.c 1.3 95/01/18 */
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

/* _ieee754_acos(x)
 * Method :
 *	fd_acos(x)  = pi/2 - fd_asin(x)
 *	fd_acos(-x) = pi/2 + fd_asin(x)
 * For |x|<=0.5
 *	fd_acos(x) = pi/2 - (x + x*x^2*R(x^2))	(see fd_asin.c)
 * For x>0.5
 * 	fd_acos(x) = pi/2 - (pi/2 - 2asin(fd_sqrt((1-x)/2)))
 *		= 2asin(fd_sqrt((1-x)/2))
 *		= 2s + 2s*z*R(z) 	...z=(1-x)/2, s=fd_sqrt(z)
 *		= 2f + (2c + 2s*z*R(z))
 *     where f=hi part of s, and c = (z-f*f)/(s+f) is the correction term
 *     for f so that f+c ~ fd_sqrt(z).
 * For x<-0.5
 *	fd_acos(x) = pi - 2asin(fd_sqrt((1-|x|)/2))
 *		= pi - 0.5*(s+s*z*R(z)), where z=(1-|x|)/2,s=fd_sqrt(z)
 *
 * Special cases:
 *	if x is NaN, return x itself;
 *	if |x|>1, return NaN with invalid signal.
 *
 * Function needed: fd_sqrt
 */

#include "fdlibm.h"
#include "fdlibm_intern.h"
