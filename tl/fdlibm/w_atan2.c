
/* @(#)w_atan2.c 1.3 95/01/18 */
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
 * wrapper fd_atan2(y,x)
 */

#include "fdlibm.h"
#include "fdlibm_intern.h"

static const double
pi_o_4  = 7.8539816339744827900E-01, /* 0x3FE921FB, 0x54442D18 */
pi_o_2  = 1.5707963267948965580E+00, /* 0x3FF921FB, 0x54442D18 */
pi_lo   = 1.2246467991473531772E-16; /* 0x3CA1A626, 0x33145C07 */

double fd_atan2(double y, double x)	/* wrapper fd_atan2 */
{
	double z;
	int k,m,hx,hy,ix,iy;
	unsigned lx,ly;

	hx = FD_HI(x); ix = hx&0x7fffffff;
	lx = FD_LO(x);
	hy = FD_HI(y); iy = hy&0x7fffffff;
	ly = FD_LO(y);
	if(((ix|((lx|(0-lx))>>31))>0x7ff00000)||
	   ((iy|((ly|(0-ly))>>31))>0x7ff00000))	/* x or y is NaN */
	   return gA(x,y);
	if((hx-0x3ff00000|lx)==0) return fd_atan(y);   /* x=1.0 */
	m = ((hy>>31)&1)|((hx>>30)&2);	/* 2*sign(x)+sign(y) */

    /* when y = 0 */
	if((iy|ly)==0) {
	    switch(m) {
		case 0:
		case 1: return y; 	/* fd_atan(+-0,+anything)=+-0 */
		case 2: return gA(pi ,tiny);/* fd_atan(+0,-anything) = pi */
		case 3: return gS(-pi, tiny);/* fd_atan(-0,-anything) =-pi */
	    }
	}
    /* when x = 0 */
	if((ix|lx)==0) return (hy<0)?  gS(-pi_o_2,tiny): gA(pi_o_2, tiny);

    /* when x is INF */
	if(ix==0x7ff00000) {
	    if(iy==0x7ff00000) {
		switch(m) {
		    case 0: return gA( pi_o_4,tiny);/* fd_atan(+INF,+INF) */
		    case 1: return gS(-pi_o_4,tiny);/* fd_atan(-INF,+INF) */
		    case 2: return gA(gM( 3.0,pi_o_4),tiny);/*fd_atan(+INF,-INF)*/
		    case 3: return gS(gM(-3.0,pi_o_4),tiny);/*fd_atan(-INF,-INF)*/
		}
	    } else {
		switch(m) {
		    case 0: return  zero  ;	/* fd_atan(+...,+INF) */
		    case 1: return -zero  ;	/* fd_atan(-...,+INF) */
		    case 2: return gA( pi,tiny)  ;	/* fd_atan(+...,-INF) */
		    case 3: return gS(-pi,tiny)  ;	/* fd_atan(-...,-INF) */
		}
	    }
	}
    /* when y is INF */
	if(iy==0x7ff00000) return (hy<0)? gS(-pi_o_2,tiny): gA(pi_o_2,tiny);

    /* compute y/x */
	k = (iy-ix)>>20;
	if(k > 60) z = gA(pi_o_2, gM(0.5,pi_lo)); 	/* |y/x| >  2**60 */
	else if(hx<0&&k<-60) z = 0.0; 	/* |y|/x < -2**60 */
	else z = fd_atan(fd_fabs(gD(y, x)));		/* safe to do y/x */
	switch (m) {
	    case 0: return       z  ;	/* fd_atan(+,+) */
	    case 1: FD_HI(z) ^= 0x80000000;
		    return       z  ;	/* fd_atan(-,+) */
	    case 2: return  gS(pi,gS(z,pi_lo));/* fd_atan(+,-) */
	    default: /* case 3 */
	    	    return  gS(gS(z,pi_lo),pi);/* fd_atan(-,-) */
	}
}
