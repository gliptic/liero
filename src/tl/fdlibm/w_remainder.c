
/* @(#)w_remainder.c 1.3 95/01/18 */
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
 * wrapper fd_remainder(x,p)
 */

#include "fdlibm.h"
#include "fdlibm_intern.h"

double fd_remainder(double x, double p)	/* wrapper fd_remainder */
{
	int hx,hp;
	unsigned sx,lx,lp;
	double p_half;

	hx = FD_HI(x);		/* high word of x */
	lx = FD_LO(x);		/* low  word of x */
	hp = FD_HI(p);		/* high word of p */
	lp = FD_LO(p);		/* low  word of p */
	sx = hx&0x80000000;
	hp &= 0x7fffffff;
	hx &= 0x7fffffff;

    /* purge off fd_exception values */
	if((hp|lp)==0) return gD(gM(x,p),gM(x,p)); 	/* p = 0 */
	if((hx>=0x7ff00000)||			/* x not fd_finite */
	  ((hp>=0x7ff00000)&&			/* p is NaN */
	  (((hp-0x7ff00000)|lp)!=0)))
	    return gD(gM(x,p),gM(x,p));


	if (hp<=0x7fdfffff) x = fd_fmod(x,gA(p,p));	/* now x < 2p */
	if (((hx-hp)|(lx-lp))==0) return gM(zero,x);
	x  = fd_fabs(x);
	p  = fd_fabs(p);
	if (hp<0x00200000) {
	    if(gA(x,x) > p) {
		x = gS(x,p);
		if(gA(x,x) >= p) x = gS(x,p);
	    }
	} else {
	    p_half = gM(0.5,p);
	    if(x>p_half) {
		x = gS(x,p);
		if(x>=p_half) x = gS(x,p);
	    }
	}
	FD_HI(x) ^= sx;
	return x;
}
