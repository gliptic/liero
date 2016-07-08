
/* @(#)s_expm1.c 1.3 95/01/18 */
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

/* fd_expm1(x)
 * Returns fd_exp(x)-1, the exponential of x minus 1.
 *
 * Method
 *   1. Argument reduction:
 *	Given x, find r and integer k such that
 *
 *               x = k*ln2 + r,  |r| <= 0.5*ln2 ~ 0.34658
 *
 *      Here a correction term c will be computed to compensate
 *	the error in r when rounded to a floating-point number.
 *
 *   2. Approximating fd_expm1(r) by a special rational function on
 *	the interval [0,0.34658]:
 *	Since
 *	    r*(fd_exp(r)+1)/(fd_exp(r)-1) = 2+ r^2/6 - r^4/360 + ...
 *	we define R1(r*r) by
 *	    r*(fd_exp(r)+1)/(fd_exp(r)-1) = 2+ r^2/6 * R1(r*r)
 *	That is,
 *	    R1(r**2) = 6/r *((fd_exp(r)+1)/(fd_exp(r)-1) - 2/r)
 *		     = 6/r * ( 1 + 2.0*(1/(fd_exp(r)-1) - 1/r))
 *		     = 1 - r^2/60 + r^4/2520 - r^6/100800 + ...
 *      We use a special Reme algorithm on [0,0.347] to generate
 * 	a polynomial of degree 5 in r*r to approximate R1. The
 *	maximum error of this polynomial approximation is bounded
 *	by 2**-61. In other words,
 *	    R1(z) ~ 1.0 + Q1*z + Q2*z**2 + Q3*z**3 + Q4*z**4 + Q5*z**5
 *	where 	Q1  =  -1.6666666666666567384E-2,
 * 		Q2  =   3.9682539681370365873E-4,
 * 		Q3  =  -9.9206344733435987357E-6,
 * 		Q4  =   2.5051361420808517002E-7,
 * 		Q5  =  -6.2843505682382617102E-9;
 *  	(where z=r*r, and the values of Q1 to Q5 are listed below)
 *	with error bounded by
 *	    |                  5           |     -61
 *	    | 1.0+Q1*z+...+Q5*z   -  R1(z) | <= 2
 *	    |                              |
 *
 *	fd_expm1(r) = fd_exp(r)-1 is then computed by the following
 * 	specific way which minimize the accumulation rounding error:
 *			       2     3
 *			      r     r    [ 3 - (R1 + R1*r/2)  ]
 *	      fd_expm1(r) = r + --- + --- * [--------------------]
 *		              2     2    [ 6 - r*(3 - R1*r/2) ]
 *
 *	To compensate the error in the argument reduction, we use
 *		fd_expm1(r+c) = fd_expm1(r) + c + fd_expm1(r)*c
 *			   ~ fd_expm1(r) + c + r*c
 *	Thus c+r*c will be added in as the correction terms for
 *	fd_expm1(r+c). Now rearrange the term to avoid optimization
 * 	screw up:
 *		        (      2                                    2 )
 *		        ({  ( r    [ R1 -  (3 - R1*r/2) ]  )  }    r  )
 *	 fd_expm1(r+c)~r - ({r*(--- * [--------------------]-c)-c} - --- )
 *	                ({  ( 2    [ 6 - r*(3 - R1*r/2) ]  )  }    2  )
 *                      (                                             )
 *
 *		   = r - E
 *   3. Scale back to obtain fd_expm1(x):
 *	From step 1, we have
 *	   fd_expm1(x) = either 2^k*[fd_expm1(r)+1] - 1
 *		    = or     2^k*[fd_expm1(r) + (1-2^-k)]
 *   4. Implementation notes:
 *	(A). To save one multiplication, we scale the coefficient Qi
 *	     to Qi*2^i, and replace z by (x^2)/2.
 *	(B). To achieve maximum accuracy, we compute fd_expm1(x) by
 *	  (i)   if x < -56*ln2, return -1.0, (raise inexact if x!=inf)
 *	  (ii)  if k=0, return r-E
 *	  (iii) if k=-1, return 0.5*(r-E)-0.5
 *        (iv)	if k=1 if r < -0.25, return 2*((r+0.5)- E)
 *	       	       else	     return  1.0+2.0*(r-E);
 *	  (v)   if (k<-2||k>56) return 2^k(1-(E-r)) - 1 (or fd_exp(x)-1)
 *	  (vi)  if k <= 20, return 2^k((1-2^-k)-(E-r)), else
 *	  (vii) return 2^k(1-((E+2^-k)-r))
 *
 * Special cases:
 *	fd_expm1(INF) is INF, fd_expm1(NaN) is NaN;
 *	fd_expm1(-INF) is -1, and
 *	for fd_finite argument, only fd_expm1(0)=0 is exact.
 *
 * Accuracy:
 *	according to an error analysis, the error is always less than
 *	1 ulp (unit in the last place).
 *
 * Misc. info.
 *	For IEEE double
 *	    if x >  7.09782712893383973096e+02 then fd_expm1(x) overflow
 *
 * Constants:
 * The hexadecimal values are the intended ones for the following
 * constants. The decimal values may be used, provided that the
 * compiler will convert from decimal to binary accurately enough
 * to produce the hexadecimal values shown.
 */

#include "fdlibm.h"
#include "fdlibm_intern.h"

static const double
	/* scaled coefficients related to fd_expm1 */
Q1  =  -3.33333333333331316428e-02, /* BFA11111 111110F4 */
Q2  =   1.58730158725481460165e-03, /* 3F5A01A0 19FE5585 */
Q3  =  -7.93650757867487942473e-05, /* BF14CE19 9EAADBB7 */
Q4  =   4.00821782732936239552e-06, /* 3ED0CFCA 86E65239 */
Q5  =  -2.01099218183624371326e-07; /* BE8AFDB7 6E09C32D */

double fd_expm1(double x)
{
	double y,hi,lo,c,t,e,hxs,hfx,r1;
	int k,xsb;
	unsigned hx;

	hx  = FD_HI(x);	/* high word of x */
	xsb = hx&0x80000000;		/* sign bit of x */
	if(xsb==0) y=x; else y= -x;	/* y = |x| */
	hx &= 0x7fffffff;		/* high word of |x| */

    /* filter out huge and non-finite argument */
	if(hx >= 0x4043687A) {			/* if |x|>=56*ln2 */
	    if(hx >= 0x40862E42) {		/* if |x|>=709.78... */
                if(hx>=0x7ff00000) {
		    if(((hx&0xfffff)|FD_LO(x))!=0)
		         return gA(x,x); 	 /* NaN */
		    else return (xsb==0)? x:-1.0;/* fd_exp(+-inf)={inf,-1} */
	        }
	        if(x > o_threshold) return gM(huge,huge); /* overflow */
	    }
	    if(xsb!=0) { /* x < -56*ln2, return -1.0 with inexact */
		if(gA(x, tiny) < 0.0)		/* raise inexact */
		return gS(tiny,one);	/* return -1 */
	    }
	}

    /* argument reduction */
	if(hx > 0x3fd62e42) {		/* if  |x| > 0.5 ln2 */
	    if(hx < 0x3FF0A2B2) {	/* and |x| < 1.5 ln2 */
		if(xsb==0)
		    {hi = gS(x, ln2_hi); lo =  ln2_lo;  k =  1;}
		else
		    {hi = gA(x, ln2_hi); lo = -ln2_lo;  k = -1;}
	    } else {
		k  = gA(gM(invln2,x), ((xsb==0)?0.5:-0.5));
		t  = k;
		hi = gS(x, gM(t,ln2_hi));	/* t*ln2_hi is exact here */
		lo = gM(t,ln2_lo);
	    }
	    x  = gS(hi, lo);
	    c  = gS(gS(hi,x),lo);
	}
	else if(hx < 0x3c900000) {  	/* when |x|<2**-54, return x */
	    t = gA(huge,x);	/* return x with inexact flags when x!=0 */
	    return gS(x, gS(t, gA(huge, x)));
	}
	else k = 0;

    /* x is now in primary range */
	hfx = gM(0.5,x);
	hxs = gM(x,hfx);
	r1 = gA(one, gM(hxs,gA(Q1, gM(hxs,gA(Q2, gM(hxs,gA(Q3, gM(hxs,gA(Q4, gM(hxs,Q5))))))))));
	t  = gS(3.0, gM(r1,hfx));
	e  = gM(hxs, gD(gS(r1, t), gS(6.0, gM(x,t))));
	if(k==0) return gS(x, gS(gM(x,e),hxs));		/* c is 0 */
	else {
	    e = gS(gM(x,gS(e, c)), c);
	    e = gS(e, hxs);
	    if(k== -1) return gS(gM(0.5, gS(x, e)), 0.5);
	    if(k==1)
	       	if(x < -0.25) return gM(-2.0, gS(e, gA(x, 0.5)));
	       	else 	      return   gA(one,  gM(2.0, gS(x, e)));
	    if (k <= -2 || k>56) {   /* suffice to return fd_exp(x)-1 */
	        y = gS(one,gS(e,x));
	        FD_HI(y) += (k<<20);	/* add k to y's exponent */
	        return gS(y,one);
	    }
	    t = one;
	    if(k<20) {
	       	FD_HI(t) = 0x3ff00000 - (0x200000>>k);  /* t=1-2^-k */
	       	y = gS(t,gS(e,x));
	       	FD_HI(y) += (k<<20);	/* add k to y's exponent */
	   } else {
	       	FD_HI(t)  = ((0x3ff-k)<<20);	/* 2^-k */
	       	y = gS(x,gA(e,t));
	       	y = gA(y,one);
	       	FD_HI(y) += (k<<20);	/* add k to y's exponent */
	    }
	}
	return y;
}
