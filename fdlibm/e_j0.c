
/* @(#)e_j0.c 1.3 95/01/18 */
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

/* _ieee754_j0(x), _ieee754_y0(x)
 * Bessel function of the first and second kinds of order zero.
 * Method -- fd_j0(x):
 *	1. For tiny x, we use fd_j0(x) = 1 - x^2/4 + x^4/64 - ...
 *	2. Reduce x to |x| since fd_j0(x)=fd_j0(-x),  and
 *	   for x in (0,2)
 *		fd_j0(x) = 1-z/4+ z^2*R0/S0,  where z = x*x;
 *	   (precision:  |fd_j0-1+z/4-z^2R0/S0 |<2**-63.67 )
 *	   for x in (2,inf)
 * 		fd_j0(x) = fd_sqrt(2/(pi*x))*(p0(x)*fd_cos(x0)-q0(x)*fd_sin(x0))
 * 	   where x0 = x-pi/4. It is better to compute fd_sin(x0),fd_cos(x0)
 *	   as follow:
 *		fd_cos(x0) = fd_cos(x)fd_cos(pi/4)+fd_sin(x)fd_sin(pi/4)
 *			= 1/fd_sqrt(2) * (fd_cos(x) + fd_sin(x))
 *		fd_sin(x0) = fd_sin(x)fd_cos(pi/4)-fd_cos(x)fd_sin(pi/4)
 *			= 1/fd_sqrt(2) * (fd_sin(x) - fd_cos(x))
 * 	   (To avoid cancellation, use
 *		fd_sin(x) +- fd_cos(x) = -fd_cos(2x)/(fd_sin(x) -+ fd_cos(x))
 * 	    to compute the worse one.)
 *
 *	3 Special cases
 *		fd_j0(nan)= nan
 *		fd_j0(0) = 1
 *		fd_j0(inf) = 0
 *
 * Method -- fd_y0(x):
 *	1. For x<2.
 *	   Since
 *		fd_y0(x) = 2/pi*(fd_j0(x)*(ln(x/2)+Euler) + x^2/4 - ...)
 *	   therefore fd_y0(x)-2/pi*fd_j0(x)*ln(x) is an even function.
 *	   We use the following function to approximate fd_y0,
 *		fd_y0(x) = U(z)/V(z) + (2/pi)*(fd_j0(x)*ln(x)), z= x^2
 *	   where
 *		U(z) = u00 + u01*z + ... + u06*z^6
 *		V(z) = 1  + v01*z + ... + v04*z^4
 *	   with absolute approximation error bounded by 2**-72.
 *	   Note: For tiny x, U/V = u0 and fd_j0(x)~1, hence
 *		fd_y0(tiny) = u0 + (2/pi)*ln(tiny), (choose tiny<2**-27)
 *	2. For x>=2.
 * 		fd_y0(x) = fd_sqrt(2/(pi*x))*(p0(x)*fd_cos(x0)+q0(x)*fd_sin(x0))
 * 	   where x0 = x-pi/4. It is better to compute fd_sin(x0),fd_cos(x0)
 *	   by the method mentioned above.
 *	3. Special cases: fd_y0(0)=-inf, fd_y0(x<0)=NaN, fd_y0(inf)=0.
 */

#include "fdlibm.h"
#include "fdlibm_intern.h"
