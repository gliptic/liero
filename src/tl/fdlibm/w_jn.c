
/* @(#)w_jn.c 1.3 95/01/18 */
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
 * wrapper fd_jn(int n, double x), fd_yn(int n, double x)
 * floating point Bessel's function of the 1st and 2nd kind
 * of order n
 *
 * Special cases:
 *	fd_y0(0)=fd_y1(0)=fd_yn(n,0) = -inf with division by zero signal;
 *	fd_y0(-ve)=fd_y1(-ve)=fd_yn(n,-ve) are NaN with invalid signal.
 * Note 2. About fd_jn(n,x), fd_yn(n,x)
 *	For n=0, fd_j0(x) is called,
 *	for n=1, fd_j1(x) is called,
 *	for n<x, forward recursion us used starting
 *	from values of fd_j0(x) and fd_j1(x).
 *	for n>x, a continued fraction approximation to
 *	j(n,x)/j(n-1,x) is evaluated and then backward
 *	recursion is used starting from a supposed value
 *	for j(n,x). The resulting value of j(0,x) is
 *	compared with the actual value to correct the
 *	supposed value of j(n,x).
 *
 *	fd_yn(n,x) is similar in all respects, except
 *	that forward recursion is used for all
 *	values of n>1.
 *
 */

#include "fdlibm.h"
#include "fdlibm_intern.h"

double fd_jn(int n, double x)	/* wrapper fd_jn */
{
	int i,hx,ix,lx, sgn;
	double a, b, temp, di;
	double z, w;

    /* J(-n,x) = (-1)^n * J(n, x), J(n, -x) = (-1)^n * J(n, x)
     * Thus, J(-n,x) = J(n,-x)
     */
	hx = FD_HI(x);
	ix = 0x7fffffff&hx;
	lx = FD_LO(x);
    /* if J(n,NaN) is NaN */
	if((ix|((unsigned)(lx|-lx))>>31)>0x7ff00000) return x+x;
	if(n<0){
		n = -n;
		x = -x;
		hx ^= 0x80000000;
	}
	if(n==0) return(fd_j0(x));
	if(n==1) return(fd_j1(x));
	sgn = (n&1)&(hx>>31);	/* even n -- 0, odd n -- sign(x) */
	x = fd_fabs(x);
	if((ix|lx)==0||ix>=0x7ff00000) 	/* if x is 0 or inf */
	    b = zero;
	else if((double)n <= x) {
		/* Safe to use J(n+1,x)=2n/x *J(n,x)-J(n-1,x) */
	    if(ix>=0x52D00000) { /* x > 2**302 */
    /* (x >> n**2)
     *	    Jn(x) = fd_cos(x-(2n+1)*pi/4)*fd_sqrt(2/x*pi)
     *	    Yn(x) = fd_sin(x-(2n+1)*pi/4)*fd_sqrt(2/x*pi)
     *	    Let s=fd_sin(x), c=fd_cos(x),
     *		xn=x-(2n+1)*pi/4, sqt2 = fd_sqrt(2),then
     *
     *		   n	fd_sin(xn)*sqt2	fd_cos(xn)*sqt2
     *		----------------------------------
     *		   0	 s-c		 c+s
     *		   1	-s-c 		-c+s
     *		   2	-s+c		-c-s
     *		   3	 s+c		 c-s
     */
		switch(n&3) {
		    case 0: temp = gA( fd_cos(x),fd_sin(x)); break;
		    case 1: temp = gA(-fd_cos(x),fd_sin(x)); break;
		    case 2: temp = gS(-fd_cos(x),fd_sin(x)); break;
		    case 3: temp = gS( fd_cos(x),fd_sin(x)); break;
		}
		b = gD(gM(invsqrtpi,temp), gSqrt(x));
	    } else {
	        a = fd_j0(x);
	        b = fd_j1(x);
	        for(i=1;i<n;i++){
		    temp = b;
		    b = gS(gM(b, gD((double)(i+i),x)), a); /* avoid underflow */
		    a = temp;
	        }
	    }
	} else {
	    if(ix<0x3e100000) {	/* x < 2**-29 */
    /* x is tiny, return the first Taylor expansion of J(n,x)
     * J(n,x) = 1/n!*(x/2)^n  - ...
     */
		if(n>33)	/* underflow */
		    b = zero;
		else {
		    temp = gM(x,0.5); b = temp;
		    for (a=one,i=2;i<=n;i++) {
			a = gM(a, (double)i);		/* a = n! */
			b = gM(b, temp);		/* b = (x/2)^n */
		    }
		    b = gD(b,a);
		}
	    } else {
		/* use backward recurrence */
		/* 			x      x^2      x^2
		 *  J(n,x)/J(n-1,x) =  ----   ------   ------   .....
		 *			2n  - 2(n+1) - 2(n+2)
		 *
		 * 			1      1        1
		 *  (for large x)   =  ----  ------   ------   .....
		 *			2n   2(n+1)   2(n+2)
		 *			-- - ------ - ------ -
		 *			 x     x         x
		 *
		 * Let w = 2n/x and h=2/x, then the above quotient
		 * is equal to the continued fraction:
		 *		    1
		 *	= -----------------------
		 *		       1
		 *	   w - -----------------
		 *			  1
		 * 	        w+h - ---------
		 *		       w+2h - ...
		 *
		 * To determine how many terms needed, let
		 * Q(0) = w, Q(1) = w(w+h) - 1,
		 * Q(k) = (w+k*h)*Q(k-1) - Q(k-2),
		 * When Q(k) > 1e4	good for single
		 * When Q(k) > 1e9	good for double
		 * When Q(k) > 1e17	good for quadruple
		 */
	    /* determine k */
		double t,v;
		double q0,q1,h,tmp; int k,m;
		w = gD((n+n), (double)x);
		h = gD(2.0,(double)x);
		q0 = w;  z = gA(w,h); q1 = gS(gM(w,z), 1.0); k=1;
		while(q1<1.0e9) {
			k += 1; z = gA(z, h);
			tmp = gS(gM(z, q1), q0);
			q0 = q1;
			q1 = tmp;
		}
		m = n+n;
		for(t=zero, i = 2*(n+k); i>=m; i -= 2) t = gD(one, gS(gD(i,x), t));
		a = t;
		b = one;
		/*  estimate fd_log((2/x)^n*n!) = n*fd_log(2/x)+n*ln(n)
		 *  Hence, if n*(fd_log(2n/x)) > ...
		 *  single 8.8722839355e+01
		 *  double 7.09782712893383973096e+02
		 *  long double 1.1356523406294143949491931077970765006170e+04
		 *  then recurrent value may overflow and the result is
		 *  likely underflow to zero
		 */
		tmp = n;
		v = gD(two,x);
		tmp = gM(tmp, fd_log(fd_fabs(gM(v,tmp))));
		if(tmp<7.09782712893383973096e+02) {
	    	    for(i=n-1,di=(double)(i+i);i>0;i--){
		        temp = b;
			b = gM(b, di);
			b  = gS(gD(b,x), a);
		        a = temp;
			di = gS(di, two);
	     	    }
		} else {
	    	    for(i=n-1,di=(double)(i+i);i>0;i--){
		        temp = b;
			b = gM(b, di);
			b  = gS(gD(b,x), a);
		        a = temp;
			di = gS(di, two);
		    /* scale b to avoid spurious overflow */
			if(b>1e100) {
			    a = gD(a, b);
			    t = gD(t, b);
			    b = one;
			}
	     	    }
		}
	    	b = gD(gM(t, fd_j0(x)), b);
	    }
	}
	if(sgn==1) return -b; else return b;
}

double fd_yn(int n, double x)	/* wrapper fd_yn */
{
	int i,hx,ix,lx;
	int sign;
	double a, b, temp;

	hx = FD_HI(x);
	ix = 0x7fffffff&hx;
	lx = FD_LO(x);
    /* if Y(n,NaN) is NaN */
	if((ix|((unsigned)(lx|-lx))>>31)>0x7ff00000) return x+x;
	if((ix|lx)==0) return gD(-one,zero);
	if(hx<0) return gD(zero,zero);
	sign = 1;
	if(n<0){
		n = -n;
		sign = 1 - ((n&1)<<1);
	}
	if(n==0) return fd_y0(x);
	if(n==1) return gM(sign, fd_y1(x));
	if(ix==0x7ff00000) return zero;
	if(ix>=0x52D00000) { /* x > 2**302 */
    /* (x >> n**2)
     *	    Jn(x) = fd_cos(x-(2n+1)*pi/4)*fd_sqrt(2/x*pi)
     *	    Yn(x) = fd_sin(x-(2n+1)*pi/4)*fd_sqrt(2/x*pi)
     *	    Let s=fd_sin(x), c=fd_cos(x),
     *		xn=x-(2n+1)*pi/4, sqt2 = fd_sqrt(2),then
     *
     *		   n	fd_sin(xn)*sqt2	fd_cos(xn)*sqt2
     *		----------------------------------
     *		   0	 s-c		 c+s
     *		   1	-s-c 		-c+s
     *		   2	-s+c		-c-s
     *		   3	 s+c		 c-s
     */
		switch(n&3) {
		    case 0: temp = gS( fd_sin(x),fd_cos(x)); break;
		    case 1: temp = gS(-fd_sin(x),fd_cos(x)); break;
		    case 2: temp = gA(-fd_sin(x),fd_cos(x)); break;
		    case 3: temp = gA( fd_sin(x),fd_cos(x)); break;
		}
		b = gD(gM(invsqrtpi,temp), gSqrt(x));
	} else {
	    a = fd_y0(x);
	    b = fd_y1(x);
	/* quit if b is -inf */
	    for(i=1;i<n&&(FD_HI(b) != 0xfff00000);i++){
		temp = b;
		b = gS(gM(gD((double)(i+i),x),b), a);
		a = temp;
	    }
	}
	if(sign>0) return b; else return -b;
}
