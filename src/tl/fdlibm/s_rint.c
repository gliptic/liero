
/* @(#)s_rint.c 1.3 95/01/18 */
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
 * fd_rint(x)
 * Return x rounded to integral value according to the prevailing
 * rounding mode.
 * Method:
 *	Using floating addition.
 * Exception:
 *	Inexact flag raised if x not equal to fd_rint(x).
 */

#include "fdlibm.h"
#include "fdlibm_intern.h"

static const double
TWO52[2]={
  4.50359962737049600000e+15, /* 0x43300000, 0x00000000 */
 -4.50359962737049600000e+15, /* 0xC3300000, 0x00000000 */
};

double fd_rint(double x)
{
	int i0,fd_j0,sx;
	unsigned i,i1;
	double w,t;
	i0 =  FD_HI(x);
	sx = (i0>>31)&1;
	i1 =  FD_LO(x);
	fd_j0 = ((i0>>20)&0x7ff)-0x3ff;
	if(fd_j0<20) {
	    if(fd_j0<0) {
		if(((i0&0x7fffffff)|i1)==0) return x;
		i1 |= (i0&0x0fffff);
		i0 &= 0xfffe0000;
		i0 |= ((i1|-i1)>>12)&0x80000;
		FD_HI(x)=i0;
	        w = gA(TWO52[sx], x);
	        t = gS(w, TWO52[sx]);
	        i0 = FD_HI(t);
	        FD_HI(t) = (i0&0x7fffffff)|(sx<<31);
	        return t;
	    } else {
		i = (0x000fffff)>>fd_j0;
		if(((i0&i)|i1)==0) return x; /* x is integral */
		i>>=1;
		if(((i0&i)|i1)!=0) {
		    if(fd_j0==19) i1 = 0x40000000; else
		    i0 = (i0&(~i))|((0x20000)>>fd_j0);
		}
	    }
	} else if (fd_j0>51) {
	    if(fd_j0==0x400) return gA(x,x);	/* inf or NaN */
	    else return x;		/* x is integral */
	} else {
	    i = ((unsigned)(0xffffffff))>>(fd_j0-20);
	    if((i1&i)==0) return x;	/* x is integral */
	    i>>=1;
	    if((i1&i)!=0) i1 = (i1&(~i))|((0x40000000)>>(fd_j0-20));
	}
	FD_HI(x) = i0;
	FD_LO(x) = i1;
	w = gA(TWO52[sx],x);
	return gS(w,TWO52[sx]);
}
