
/* @(#)w_hypot.c 1.3 95/01/18 */
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
 * wrapper fd_hypot(x,y)
 */

#include "fdlibm.h"
#include "fdlibm_intern.h"

double fd_hypot(double x, double y)/* wrapper fd_hypot */
{
	double a=x,b=y,t1,t2,fd_y1,y2,w;
	int j,k,ha,hb;

	ha = FD_HI(x)&0x7fffffff;	/* high word of  x */
	hb = FD_HI(y)&0x7fffffff;	/* high word of  y */
	if(hb > ha) {a=y;b=x;j=ha; ha=hb;hb=j;} else {a=x;b=y;}
	FD_HI(a) = ha;	/* a <- |a| */
	FD_HI(b) = hb;	/* b <- |b| */
	if((ha-hb)>0x3c00000) {return gA(a,b);} /* x/y > 2**60 */
	k=0;
	if(ha > 0x5f300000) {	/* a>2**500 */
	   if(ha >= 0x7ff00000) {	/* Inf or NaN */
	       w = gA(a,b);			/* for sNaN */
	       if(((ha&0xfffff)|FD_LO(a))==0) w = a;
	       if(((hb^0x7ff00000)|FD_LO(b))==0) w = b;
	       return w;
	   }
	   /* scale a and b by 2**-600 */
	   ha -= 0x25800000; hb -= 0x25800000;	k += 600;
	   FD_HI(a) = ha;
	   FD_HI(b) = hb;
	}
	if(hb < 0x20b00000) {	/* b < 2**-500 */
	    if(hb <= 0x000fffff) {	/* subnormal b or 0 */
		if((hb|(FD_LO(b)))==0) return a;
		t1=0;
		FD_HI(t1) = 0x7fd00000;	/* t1=2^1022 */
		b = gM(b,t1);
		a = gM(a,t1);
		k -= 1022;
	    } else {		/* scale a and b by 2^600 */
	        ha += 0x25800000; 	/* a *= 2^600 */
		hb += 0x25800000;	/* b *= 2^600 */
		k -= 600;
	   	FD_HI(a) = ha;
	   	FD_HI(b) = hb;
	    }
	}
    /* medium size a and b */
	w = gS(a,b);
	if (w>b) {
	    t1 = 0;
	    FD_HI(t1) = ha;
	    t2 = gS(a,t1);
	    w  = gSqrt(gS(gM(t1,t1), gS( gM(b,(-b)), gM(t2,gA(a,t1)) )));
	} else {
	    a  = gA(a,a);
	    fd_y1 = 0;
	    FD_HI(fd_y1) = hb;
	    y2 = gS(b, fd_y1);
	    t1 = 0;
	    FD_HI(t1) = ha+0x00100000;
	    t2 = gS(a, t1);
	    w  = gSqrt(gS(gM(t1,fd_y1), gS( gM(w,(-w)), gA(gM(t1,y2), gM(t2,b)) )));
	}
	if(k!=0) {
	    t1 = 1.0;
	    FD_HI(t1) += (k<<20);
	    return gM(t1,w);
	} else return w;
}
