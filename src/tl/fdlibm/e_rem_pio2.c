
/* @(#)e_rem_pio2.c 1.4 95/01/18 */
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

/* _ieee754_rem_pio2(x,y)
 *
 * return the fd_remainder of x rem pi/2 in y[0]+y[1]
 * use _kernel_rem_pio2()
 */

#include "fdlibm.h"
#include "fdlibm_intern.h"

/*
 * Table of constants for 2/pi, 396 Hex digits (476 decimal) of 2/pi
 */

static const int two_over_pi[] = {
0xA2F983, 0x6E4E44, 0x1529FC, 0x2757D1, 0xF534DD, 0xC0DB62,
0x95993C, 0x439041, 0xFE5163, 0xABDEBB, 0xC561B7, 0x246E3A,
0x424DD2, 0xE00649, 0x2EEA09, 0xD1921C, 0xFE1DEB, 0x1CB129,
0xA73EE8, 0x8235F5, 0x2EBB44, 0x84E99C, 0x7026B4, 0x5F7E41,
0x3991D6, 0x398353, 0x39F49C, 0x845F8B, 0xBDF928, 0x3B1FF8,
0x97FFDE, 0x05980F, 0xEF2F11, 0x8B5A0A, 0x6D1F6D, 0x367ECF,
0x27CB09, 0xB74F46, 0x3F669E, 0x5FEA2D, 0x7527BA, 0xC7EBE5,
0xF17B3D, 0x0739F7, 0x8A5292, 0xEA6BFB, 0x5FB11F, 0x8D5D08,
0x560330, 0x46FC7B, 0x6BABF0, 0xCFBC20, 0x9AF436, 0x1DA9E3,
0x91615E, 0xE61B08, 0x659985, 0x5F14A0, 0x68408D, 0xFFD880,
0x4D7327, 0x310606, 0x1556CA, 0x73A8C9, 0x60E27B, 0xC08C6B,
};

static const int npio2_hw[] = {
0x3FF921FB, 0x400921FB, 0x4012D97C, 0x401921FB, 0x401F6A7A, 0x4022D97C,
0x4025FDBB, 0x402921FB, 0x402C463A, 0x402F6A7A, 0x4031475C, 0x4032D97C,
0x40346B9C, 0x4035FDBB, 0x40378FDB, 0x403921FB, 0x403AB41B, 0x403C463A,
0x403DD85A, 0x403F6A7A, 0x40407E4C, 0x4041475C, 0x4042106C, 0x4042D97C,
0x4043A28C, 0x40446B9C, 0x404534AC, 0x4045FDBB, 0x4046C6CB, 0x40478FDB,
0x404858EB, 0x404921FB,
};

/*
 * invpio2:  53 bits of 2/pi
 * pio2_1:   first  33 bit of pi/2
 * pio2_1t:  pi/2 - pio2_1
 * pio2_2:   second 33 bit of pi/2
 * pio2_2t:  pi/2 - (pio2_1+pio2_2)
 * pio2_3:   third  33 bit of pi/2
 * pio2_3t:  pi/2 - (pio2_1+pio2_2+pio2_3)
 */

static const double
invpio2 =  6.36619772367581382433e-01, /* 0x3FE45F30, 0x6DC9C883 */
pio2_1  =  1.57079632673412561417e+00, /* 0x3FF921FB, 0x54400000 */
pio2_1t =  6.07710050650619224932e-11, /* 0x3DD0B461, 0x1A626331 */
pio2_2  =  6.07710050630396597660e-11, /* 0x3DD0B461, 0x1A600000 */
pio2_2t =  2.02226624879595063154e-21, /* 0x3BA3198A, 0x2E037073 */
pio2_3  =  2.02226624871116645580e-21, /* 0x3BA3198A, 0x2E000000 */
pio2_3t =  8.47842766036889956997e-32; /* 0x397B839A, 0x252049C1 */

FDLIBM_INTERNAL int _ieee754_rem_pio2(double x, double *y)
{
	double z,w,t,r,fn;
	double tx[3];
	int e0,i,j,nx,n,ix,hx;

	hx = FD_HI(x);		/* high word of x */
	ix = hx&0x7fffffff;
	if(ix<=0x3fe921fb)   /* |x| ~<= pi/4 , no need for reduction */
	    {y[0] = x; y[1] = 0; return 0;}
	if(ix<0x4002d97c) {  /* |x| < 3pi/4, special case with n=+-1 */
	    if(hx>0) {
		z = gS(x, pio2_1);
		if(ix!=0x3ff921fb) { 	/* 33+53 bit pi is good enough */
		    y[0] = gS(z, pio2_1t);
		    y[1] = gS(gS(z, y[0]), pio2_1t);
		} else {		/* near pi/2, use 33+33+53 bit pi */
		    z = gS(z, pio2_2);
		    y[0] = gS(z, pio2_2t);
		    y[1] = gS(gS(z, y[0]), pio2_2t);
		}
		return 1;
	    } else {	/* negative x */
		z = gA(x, pio2_1);
		if(ix!=0x3ff921fb) { 	/* 33+53 bit pi is good enough */
		    y[0] = gA(z, pio2_1t);
		    y[1] = gA(gS(z, y[0]), pio2_1t);
		} else {		/* near pi/2, use 33+33+53 bit pi */
		    z = gA(z, pio2_2);
		    y[0] = gA(z, pio2_2t);
		    y[1] = gA(gS(z, y[0]), pio2_2t);
		}
		return -1;
	    }
	}
	if(ix<=0x413921fb) { /* |x| ~<= 2^19*(pi/2), medium size */
	    t  = fd_fabs(x);
	    n  = (int) gA(gM(t,invpio2), half);
	    fn = (double)n;
	    r  = gS(t, gM(fn,pio2_1));
	    w  = gM(fn, pio2_1t);	/* 1st round good to 85 bit */
	    if(n<32&&ix!=npio2_hw[n-1]) {
		y[0] = gS(r,w);	/* quick check no cancellation */
	    } else {
	        j  = ix>>20;
	        y[0] = gS(r,w);
	        i = j-(((FD_HI(y[0]))>>20)&0x7ff);
	        if(i>16) {  /* 2nd iteration needed, good to 118 */
		    t  = r;
		    w  = gM(fn, pio2_2);
		    r  = gS(t, w);
		    w  = gS(gM(fn, pio2_2t), gS(gS(t, r), w));
		    y[0] = gS(r,w);
		    i = j-(((FD_HI(y[0]))>>20)&0x7ff);
		    if(i>49)  {	/* 3rd iteration need, 151 bits acc */
		    	t  = r;	/* will cover all possible cases */
		    	w  = gM(fn,pio2_3);
		    	r  = gS(t,w);
		    	w  = gS(gM(fn,pio2_3t), gS(gS(t,r),w));
		    	y[0] = gS(r,w);
		    }
		}
	    }
	    y[1] = gS(gS(r,y[0]),w);
	    if(hx<0) 	{y[0] = -y[0]; y[1] = -y[1]; return -n;}
	    else	 return n;
	}
    /*
     * all other (large) arguments
     */
	if(ix>=0x7ff00000) {		/* x is inf or NaN */
	    y[0] = y[1] = gS(x,x); return 0;
	}
    /* set z = fd_scalbn(|x|,fd_ilogb(x)-23) */
	FD_LO(z) = FD_LO(x);
	e0 	= (ix>>20)-1046;	/* e0 = fd_ilogb(z)-23; */
	FD_HI(z) = ix - (e0<<20);
	for(i=0;i<2;i++) {
		tx[i] = (double)((int)(z));
		z     = gM(gS(z, tx[i]), two24);
	}
	tx[2] = z;
	nx = 3;
	while(tx[nx-1]==zero) nx--;	/* skip zero term */
	n  =  _kernel_rem_pio2(tx,y,e0,nx,2,two_over_pi);
	if(hx<0) {y[0] = -y[0]; y[1] = -y[1]; return -n;}
	return n;
}
