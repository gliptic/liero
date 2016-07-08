
/* @(#)e_lgamma_r.c 1.3 95/01/18 */
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

/* _ieee754_lgamma_r(x, signgamp)
 * Reentrant version of the logarithm of the Gamma function
 * with user provide pointer for the sign of Gamma(x).
 *
 * Method:
 *   1. Argument Reduction for 0 < x <= 8
 * 	Since fd_gamma(1+s)=s*fd_gamma(s), for x in [0,8], we may
 * 	reduce x to a number in [1.5,2.5] by
 * 		fd_lgamma(1+s) = fd_log(s) + fd_lgamma(s)
 *	for example,
 *		fd_lgamma(7.3) = fd_log(6.3) + fd_lgamma(6.3)
 *			    = fd_log(6.3*5.3) + fd_lgamma(5.3)
 *			    = fd_log(6.3*5.3*4.3*3.3*2.3) + fd_lgamma(2.3)
 *   2. Polynomial approximation of fd_lgamma around its
 *	minimun ymin=1.461632144968362245 to maintain monotonicity.
 *	On [ymin-0.23, ymin+0.27] (i.e., [1.23164,1.73163]), use
 *		Let z = x-ymin;
 *		fd_lgamma(x) = -1.214862905358496078218 + z^2*poly(z)
 *	where
 *		poly(z) is a 14 degree polynomial.
 *   2. Rational approximation in the primary interval [2,3]
 *	We use the following approximation:
 *		s = x-2.0;
 *		fd_lgamma(x) = 0.5*s + s*P(s)/Q(s)
 *	with accuracy
 *		|P/Q - (fd_lgamma(x)-0.5s)| < 2**-61.71
 *	Our algorithms are based on the following observation
 *
 *                             zeta(2)-1    2    zeta(3)-1    3
 * fd_lgamma(2+s) = s*(1-Euler) + --------- * s  -  --------- * s  + ...
 *                                 2                 3
 *
 *	where Euler = 0.5771... is the Euler constant, which is very
 *	close to 0.5.
 *
 *   3. For x>=8, we have
 *	fd_lgamma(x)~(x-0.5)fd_log(x)-x+0.5*fd_log(2pi)+1/(12x)-1/(360x**3)+....
 *	(better formula:
 *	   fd_lgamma(x)~(x-0.5)*(fd_log(x)-1)-.5*(fd_log(2pi)-1) + ...)
 *	Let z = 1/x, then we approximation
 *		f(z) = fd_lgamma(x) - (x-0.5)(fd_log(x)-1)
 *	by
 *	  			    3       5             11
 *		w = w0 + w1*z + w2*z  + w3*z  + ... + w6*z
 *	where
 *		|w - f(z)| < 2**-58.74
 *
 *   4. For negative x, since (G is fd_gamma function)
 *		-x*G(-x)*G(x) = pi/fd_sin(pi*x),
 * 	we have
 * 		G(x) = pi/(fd_sin(pi*x)*(-x)*G(-x))
 *	since G(-x) is positive, sign(G(x)) = sign(fd_sin(pi*x)) for x<0
 *	Hence, for x<0, fd_signgam = sign(fd_sin(pi*x)) and
 *		fd_lgamma(x) = fd_log(|Gamma(x)|)
 *			  = fd_log(pi/(|x*fd_sin(pi*x)|)) - fd_lgamma(-x);
 *	Note: one should avoid compute pi*(-x) directly in the
 *	      computation of fd_sin(pi*(-x)).
 *
 *   5. Special Cases
 *		fd_lgamma(2+s) ~ s*(1-Euler) for tiny s
 *		fd_lgamma(1)=fd_lgamma(2)=0
 *		fd_lgamma(x) ~ -fd_log(x) for tiny x
 *		fd_lgamma(0) = fd_lgamma(inf) = inf
 *	 	fd_lgamma(-integer) = +-inf
 *
 */

#include "fdlibm.h"
#include "fdlibm_intern.h"
