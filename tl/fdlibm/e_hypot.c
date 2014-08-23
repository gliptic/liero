
/* @(#)e_hypot.c 1.3 95/01/18 */
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

/* _ieee754_hypot(x,y)
 *
 * Method :
 *	If (assume round-to-nearest) z=x*x+y*y
 *	has error less than fd_sqrt(2)/2 ulp, than
 *	fd_sqrt(z) has error less than 1 ulp (exercise).
 *
 *	So, compute fd_sqrt(x*x+y*y) with some care as
 *	follows to get the error below 1 ulp:
 *
 *	Assume x>y>0;
 *	(if possible, set rounding to round-to-nearest)
 *	1. if x > 2y  use
 *		x1*x1+(y*y+(x2*(x+x1))) for x*x+y*y
 *	where x1 = x with lower 32 bits cleared, x2 = x-x1; else
 *	2. if x <= 2y use
 *		t1*fd_y1+((x-y)*(x-y)+(t1*y2+t2*y))
 *	where t1 = 2x with lower 32 bits cleared, t2 = 2x-t1,
 *	fd_y1= y with lower 32 bits chopped, y2 = y-fd_y1.
 *
 *	NOTE: scaling may be necessary if some argument is too
 *	      large or too tiny
 *
 * Special cases:
 *	fd_hypot(x,y) is INF if x or y is +INF or -INF; else
 *	fd_hypot(x,y) is NAN if x or y is NAN.
 *
 * Accuracy:
 * 	fd_hypot(x,y) returns fd_sqrt(x^2+y^2) with error less
 * 	than 1 ulps (units in the last place)
 */

#include "fdlibm.h"
#include "fdlibm_intern.h"
