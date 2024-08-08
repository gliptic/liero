
/* @(#)s_nextafter.c 1.3 95/01/18 */
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

/* IEEE functions
 *	fd_nextafter(x,y)
 *	return the next machine floating-point number of x in the
 *	direction toward y.
 *   Special cases:
 */

#include "fdlibm.h"
#include "fdlibm_intern.h"

double fd_nextafter(double x, double y)
{
	int	hx,hy,ix,iy;
	unsigned lx,ly;

	hx = FD_HI(x);		/* high word of x */
	lx = FD_LO(x);		/* low  word of x */
	hy = FD_HI(y);		/* high word of y */
	ly = FD_LO(y);		/* low  word of y */
	ix = hx&0x7fffffff;		/* |x| */
	iy = hy&0x7fffffff;		/* |y| */

	if(((ix>=0x7ff00000)&&((ix-0x7ff00000)|lx)!=0) ||   /* x is nan */
	   ((iy>=0x7ff00000)&&((iy-0x7ff00000)|ly)!=0))     /* y is nan */
	   return x+y;
	if(x==y) return x;		/* x=y, return x */
	if((ix|lx)==0) {			/* x == 0 */
	    FD_HI(x) = hy&0x80000000;	/* return +-minsubnormal */
	    FD_LO(x) = 1;
	    y = x*x;
	    if(y==x) return y; else return x;	/* raise underflow flag */
	}
	if(hx>=0) {				/* x > 0 */
	    if(hx>hy||((hx==hy)&&(lx>ly))) {	/* x > y, x -= ulp */
		if(lx==0) hx -= 1;
		lx -= 1;
	    } else {				/* x < y, x += ulp */
		lx += 1;
		if(lx==0) hx += 1;
	    }
	} else {				/* x < 0 */
	    if(hy>=0||hx>hy||((hx==hy)&&(lx>ly))){/* x < y, x -= ulp */
		if(lx==0) hx -= 1;
		lx -= 1;
	    } else {				/* x > y, x += ulp */
		lx += 1;
		if(lx==0) hx += 1;
	    }
	}
	hy = hx&0x7ff00000;
	if(hy>=0x7ff00000) return gA(x,x);	/* overflow  */
	if(hy<0x00100000) {		/* underflow */
	    y = gM(x,x);
	    if(y!=x) {		/* raise underflow flag */
		FD_HI(y) = hx; FD_LO(y) = lx;
		return y;
	    }
	}
	FD_HI(x) = hx; FD_LO(x) = lx;
	return x;
}
