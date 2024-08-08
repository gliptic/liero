
/* @(#)s_modf.c 1.3 95/01/18 */
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
 * fd_modf(double x, double *iptr)
 * return fraction part of x, and return x's integral part in *iptr.
 * Method:
 *	Bit twiddling.
 *
 * Exception:
 *	No fd_exception.
 */

#include "fdlibm.h"
#include "fdlibm_intern.h"

double fd_modf(double x, double *iptr)
{
	int i0,i1,fd_j0;
	unsigned i;
	i0 =  FD_HI(x);		/* high x */
	i1 =  FD_LO(x);		/* low  x */
	fd_j0 = ((i0>>20)&0x7ff)-0x3ff;	/* exponent of x */
	if(fd_j0<20) {			/* integer part in high x */
	    if(fd_j0<0) {			/* |x|<1 */
		FD_HIp(iptr) = i0&0x80000000;
		FD_LOp(iptr) = 0;		/* *iptr = +-0 */
		return x;
	    } else {
		i = (0x000fffff)>>fd_j0;
		if(((i0&i)|i1)==0) {		/* x is integral */
		    *iptr = x;
		    FD_HI(x) &= 0x80000000;
		    FD_LO(x)  = 0;	/* return +-0 */
		    return x;
		} else {
		    FD_HIp(iptr) = i0&(~i);
		    FD_LOp(iptr) = 0;
		    return gS(x, *iptr);
		}
	    }
	} else if (fd_j0>51) {		/* no fraction part */
	    *iptr = gM(x,one);
	    FD_HI(x) &= 0x80000000;
	    FD_LO(x)  = 0;	/* return +-0 */
	    return x;
	} else {			/* fraction part in low x */
	    i = ((unsigned)(0xffffffff))>>(fd_j0-20);
	    if((i1&i)==0) { 		/* x is integral */
		*iptr = x;
		FD_HI(x) &= 0x80000000;
		FD_LO(x)  = 0;	/* return +-0 */
		return x;
	    } else {
		FD_HIp(iptr) = i0;
		FD_LOp(iptr) = i1&(~i);
		return gS(x, *iptr);
	    }
	}
}
