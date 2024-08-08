
/* @(#)s_floor.c 1.3 95/01/18 */
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
 * fd_floor(x)
 * Return x rounded toward -inf to integral value
 * Method:
 *	Bit twiddling.
 * Exception:
 *	Inexact flag raised if x not equal to fd_floor(x).
 */

#include "fdlibm.h"
#include "fdlibm_intern.h"

double fd_floor(double x)
{
	int i0,i1,fd_j0;
	unsigned i,j;
	i0 =  FD_HI(x);
	i1 =  FD_LO(x);
	fd_j0 = ((i0>>20)&0x7ff)-0x3ff;
	if(fd_j0<20) {
	    if(fd_j0<0) { 	/* raise inexact if x != 0 */
		if(gA(huge, x) > 0.0) {/* return 0*sign(x) if |x|<1 */
		    if(i0>=0) {i0=i1=0;}
		    else if(((i0&0x7fffffff)|i1)!=0)
			{ i0=0xbff00000;i1=0;}
		}
	    } else {
		i = (0x000fffff)>>fd_j0;
		if(((i0&i)|i1)==0) return x; /* x is integral */
		if(huge+x>0.0) {	/* raise inexact flag */
		    if(i0<0) i0 += (0x00100000)>>fd_j0;
		    i0 &= (~i); i1=0;
		}
	    }
	} else if (fd_j0>51) {
	    if(fd_j0==0x400) return gA(x,x);	/* inf or NaN */
	    else return x;		/* x is integral */
	} else {
	    i = ((unsigned)(0xffffffff))>>(fd_j0-20);
	    if((i1&i)==0) return x;	/* x is integral */
	    if(gA(huge, x) > 0.0) { 		/* raise inexact flag */
		if(i0<0) {
		    if(fd_j0==20) i0+=1;
		    else {
			j = i1+(1<<(52-fd_j0));
			if(j<i1) i0 +=1 ; 	/* got a carry */
			i1=j;
		    }
		}
		i1 &= (~i);
	    }
	}
	FD_HI(x) = i0;
	FD_LO(x) = i1;
	return x;
}
