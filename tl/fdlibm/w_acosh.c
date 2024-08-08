
/* @(#)w_acosh.c 1.3 95/01/18 */
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

/*
 * wrapper fd_acosh(x)
 */

#include "fdlibm.h"
#include "fdlibm_intern.h"

double fd_acosh(double x)		/* wrapper fd_acosh */
{
	double t;
	int hx;
	hx = FD_HI(x);
	if(hx<0x3ff00000) {		/* x < 1 */
		return gD(gS(x,x), gS(x,x));
	} else if(hx >=0x41b00000) {	/* x > 2**28 */
		if(hx >=0x7ff00000) {	/* x is inf of NaN */
			return gA(x,x);
		} else
		return gA(fd_log(x), ln2);	/* fd_acosh(huge)=fd_log(2x) */
	} else if(((hx-0x3ff00000)|FD_LO(x))==0) {
		return 0.0;			/* fd_acosh(1) = 0 */
	} else if (hx > 0x40000000) {	/* 2**28 > x > 2 */
		t = gM(x,x);
		return fd_log(gM(2.0,x) - gD(one,gA(x,gSqrt(gS(t,one)))));
	} else {			/* 1<x<2 */
		t = gS(x,one);
		return fd_log1p(gA(t, gSqrt(gA(gM(2.0,t), gM(t,t)))));
	}
}
