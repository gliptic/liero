
/* @(#)s_erf.c 1.3 95/01/18 */
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

/* double fd_erf(double x)
 * double fd_erfc(double x)
 *			     x
 *		      2      |\
 *     fd_erf(x)  =  ---------  | fd_exp(-t*t)dt
 *	 	   fd_sqrt(pi) \|
 *			     0
 *
 *     fd_erfc(x) =  1-fd_erf(x)
 *  Note that
 *		fd_erf(-x) = -fd_erf(x)
 *		fd_erfc(-x) = 2 - fd_erfc(x)
 *
 * Method:
 *	1. For |x| in [0, 0.84375]
 *	    fd_erf(x)  = x + x*R(x^2)
 *          fd_erfc(x) = 1 - fd_erf(x)           if x in [-.84375,0.25]
 *                  = 0.5 + ((0.5-x)-x*r)  if x in [0.25,0.84375]
 *	   where r = P/Q where P is an odd poly of degree 8 and
 *	   Q is an odd poly of degree 10.
 *						 -57.90
 *			| r - (fd_erf(x)-x)/x | <= 2
 *
 *
 *	   Remark. The formula is derived by noting
 *          fd_erf(x) = (2/fd_sqrt(pi))*(x - x^3/3 + x^5/10 - x^7/42 + ....)
 *	   and that
 *          2/fd_sqrt(pi) = 1.128379167095512573896158903121545171688
 *	   is close to one. The interval is chosen because the fix
 *	   point of fd_erf(x) is near 0.6174 (i.e., fd_erf(x)=x when x is
 *	   near 0.6174), and by some experiment, 0.84375 is chosen to
 * 	   guarantee the error is less than one ulp for fd_erf.
 *
 *      2. For |x| in [0.84375,1.25], let s = |x| - 1, and
 *         c = 0.84506291151 rounded to single (24 bits)
 *         	fd_erf(x)  = sign(x) * (c  + P1(s)/Q1(s))
 *         	fd_erfc(x) = (1-c)  - P1(s)/Q1(s) if x > 0
 *			  1+(c+P1(s)/Q1(s))    if x < 0
 *         	|P1/Q1 - (fd_erf(|x|)-c)| <= 2**-59.06
 *	   Remark: here we use the taylor series expansion at x=1.
 *		fd_erf(1+s) = fd_erf(1) + s*Poly(s)
 *			 = 0.845.. + P1(s)/Q1(s)
 *	   That is, we use rational approximation to approximate
 *			fd_erf(1+s) - (c = (single)0.84506291151)
 *	   Note that |P1/Q1|< 0.078 for x in [0.84375,1.25]
 *	   where
 *		P1(s) = degree 6 poly in s
 *		Q1(s) = degree 6 poly in s
 *
 *      3. For x in [1.25,1/0.35(~2.857143)],
 *         	fd_erfc(x) = (1/x)*fd_exp(-x*x-0.5625+R1/S1)
 *         	fd_erf(x)  = 1 - fd_erfc(x)
 *	   where
 *		R1(z) = degree 7 poly in z, (z=1/x^2)
 *		S1(z) = degree 8 poly in z
 *
 *      4. For x in [1/0.35,28]
 *         	fd_erfc(x) = (1/x)*fd_exp(-x*x-0.5625+R2/S2) if x > 0
 *			= 2.0 - (1/x)*fd_exp(-x*x-0.5625+R2/S2) if -6<x<0
 *			= 2.0 - tiny		(if x <= -6)
 *         	fd_erf(x)  = sign(x)*(1.0 - fd_erfc(x)) if x < 6, else
 *         	fd_erf(x)  = sign(x)*(1.0 - tiny)
 *	   where
 *		R2(z) = degree 6 poly in z, (z=1/x^2)
 *		S2(z) = degree 7 poly in z
 *
 *      Note1:
 *	   To compute fd_exp(-x*x-0.5625+r/S), let s be a single
 *	   precision number and s := x; then
 *		-x*x = -s*s + (s-x)*(s+x)
 *	        fd_exp(-x*x-0.5626+r/S) =
 *			fd_exp(-s*s-0.5625)*fd_exp((s-x)*(s+x)+r/S);
 *      Note2:
 *	   Here 4 and 5 make use of the asymptotic series
 *			  fd_exp(-x*x)
 *		fd_erfc(x) ~ ---------- * ( 1 + Poly(1/x^2) )
 *			  x*fd_sqrt(pi)
 *	   We use rational approximation to approximate
 *      	g(s)=f(1/x^2) = fd_log(fd_erfc(x)*x) - x*x + 0.5625
 *	   Here is the error bound for R1/S1 and R2/S2
 *      	|R1/S1 - f(x)|  < 2**(-62.57)
 *      	|R2/S2 - f(x)|  < 2**(-61.52)
 *
 *      5. For inf > x >= 28
 *         	fd_erf(x)  = sign(x) *(1 - tiny)  (raise inexact)
 *         	fd_erfc(x) = tiny*tiny (raise underflow) if x > 0
 *			= 2 - tiny if x<0
 *
 *      7. Special case:
 *         	fd_erf(0)  = 0, fd_erf(inf)  = 1, fd_erf(-inf) = -1,
 *         	fd_erfc(0) = 1, fd_erfc(inf) = 0, fd_erfc(-inf) = 2,
 *	   	fd_erfc/fd_erf(NaN) is NaN
 */


#include "fdlibm.h"
#include "fdlibm_intern.h"

static const double
	/* c = (float)0.84506291151 */
erx =  8.45062911510467529297e-01, /* 0x3FEB0AC1, 0x60000000 */
/*
 * Coefficients for approximation to  fd_erf on [0,0.84375]
 */
efx =  1.28379167095512586316e-01, /* 0x3FC06EBA, 0x8214DB69 */
efx8=  1.02703333676410069053e+00, /* 0x3FF06EBA, 0x8214DB69 */
pp0  =  1.28379167095512558561e-01, /* 0x3FC06EBA, 0x8214DB68 */
pp1  = -3.25042107247001499370e-01, /* 0xBFD4CD7D, 0x691CB913 */
pp2  = -2.84817495755985104766e-02, /* 0xBF9D2A51, 0xDBD7194F */
pp3  = -5.77027029648944159157e-03, /* 0xBF77A291, 0x236668E4 */
pp4  = -2.37630166566501626084e-05, /* 0xBEF8EAD6, 0x120016AC */
qq1  =  3.97917223959155352819e-01, /* 0x3FD97779, 0xCDDADC09 */
qq2  =  6.50222499887672944485e-02, /* 0x3FB0A54C, 0x5536CEBA */
qq3  =  5.08130628187576562776e-03, /* 0x3F74D022, 0xC4D36B0F */
qq4  =  1.32494738004321644526e-04, /* 0x3F215DC9, 0x221C1A10 */
qq5  = -3.96022827877536812320e-06, /* 0xBED09C43, 0x42A26120 */
/*
 * Coefficients for approximation to  fd_erf  in [0.84375,1.25]
 */
pa0  = -2.36211856075265944077e-03, /* 0xBF6359B8, 0xBEF77538 */
pa1  =  4.14856118683748331666e-01, /* 0x3FDA8D00, 0xAD92B34D */
pa2  = -3.72207876035701323847e-01, /* 0xBFD7D240, 0xFBB8C3F1 */
pa3  =  3.18346619901161753674e-01, /* 0x3FD45FCA, 0x805120E4 */
pa4  = -1.10894694282396677476e-01, /* 0xBFBC6398, 0x3D3E28EC */
pa5  =  3.54783043256182359371e-02, /* 0x3FA22A36, 0x599795EB */
pa6  = -2.16637559486879084300e-03, /* 0xBF61BF38, 0x0A96073F */
qa1  =  1.06420880400844228286e-01, /* 0x3FBB3E66, 0x18EEE323 */
qa2  =  5.40397917702171048937e-01, /* 0x3FE14AF0, 0x92EB6F33 */
qa3  =  7.18286544141962662868e-02, /* 0x3FB2635C, 0xD99FE9A7 */
qa4  =  1.26171219808761642112e-01, /* 0x3FC02660, 0xE763351F */
qa5  =  1.36370839120290507362e-02, /* 0x3F8BEDC2, 0x6B51DD1C */
qa6  =  1.19844998467991074170e-02, /* 0x3F888B54, 0x5735151D */
/*
 * Coefficients for approximation to  fd_erfc in [1.25,1/0.35]
 */
ra0  = -9.86494403484714822705e-03, /* 0xBF843412, 0x600D6435 */
ra1  = -6.93858572707181764372e-01, /* 0xBFE63416, 0xE4BA7360 */
ra2  = -1.05586262253232909814e+01, /* 0xC0251E04, 0x41B0E726 */
ra3  = -6.23753324503260060396e+01, /* 0xC04F300A, 0xE4CBA38D */
ra4  = -1.62396669462573470355e+02, /* 0xC0644CB1, 0x84282266 */
ra5  = -1.84605092906711035994e+02, /* 0xC067135C, 0xEBCCABB2 */
ra6  = -8.12874355063065934246e+01, /* 0xC0545265, 0x57E4D2F2 */
ra7  = -9.81432934416914548592e+00, /* 0xC023A0EF, 0xC69AC25C */
sa1  =  1.96512716674392571292e+01, /* 0x4033A6B9, 0xBD707687 */
sa2  =  1.37657754143519042600e+02, /* 0x4061350C, 0x526AE721 */
sa3  =  4.34565877475229228821e+02, /* 0x407B290D, 0xD58A1A71 */
sa4  =  6.45387271733267880336e+02, /* 0x40842B19, 0x21EC2868 */
sa5  =  4.29008140027567833386e+02, /* 0x407AD021, 0x57700314 */
sa6  =  1.08635005541779435134e+02, /* 0x405B28A3, 0xEE48AE2C */
sa7  =  6.57024977031928170135e+00, /* 0x401A47EF, 0x8E484A93 */
sa8  = -6.04244152148580987438e-02, /* 0xBFAEEFF2, 0xEE749A62 */
/*
 * Coefficients for approximation to  fd_erfc in [1/.35,28]
 */
rb0  = -9.86494292470009928597e-03, /* 0xBF843412, 0x39E86F4A */
rb1  = -7.99283237680523006574e-01, /* 0xBFE993BA, 0x70C285DE */
rb2  = -1.77579549177547519889e+01, /* 0xC031C209, 0x555F995A */
rb3  = -1.60636384855821916062e+02, /* 0xC064145D, 0x43C5ED98 */
rb4  = -6.37566443368389627722e+02, /* 0xC083EC88, 0x1375F228 */
rb5  = -1.02509513161107724954e+03, /* 0xC0900461, 0x6A2E5992 */
rb6  = -4.83519191608651397019e+02, /* 0xC07E384E, 0x9BDC383F */
sb1  =  3.03380607434824582924e+01, /* 0x403E568B, 0x261D5190 */
sb2  =  3.25792512996573918826e+02, /* 0x40745CAE, 0x221B9F0A */
sb3  =  1.53672958608443695994e+03, /* 0x409802EB, 0x189D5118 */
sb4  =  3.19985821950859553908e+03, /* 0x40A8FFB7, 0x688C246A */
sb5  =  2.55305040643316442583e+03, /* 0x40A3F219, 0xCEDF3BE6 */
sb6  =  4.74528541206955367215e+02, /* 0x407DA874, 0xE79FE763 */
sb7  = -2.24409524465858183362e+01; /* 0xC03670E2, 0x42712D62 */

double fd_erf(double x)
{
	int hx,ix,i;
	double r2,S,P,Q,s,y,z,r;
	hx = FD_HI(x);
	ix = hx&0x7fffffff;
	if(ix>=0x7ff00000) {		/* fd_erf(nan)=nan */
	    i = ((unsigned)hx>>31)<<1;
	    return gA((double)(1-i), gD(one,x));	/* fd_erf(+-inf)=+-1 */
	}

	if(ix < 0x3feb0000) {		/* |x|<0.84375 */
	    if(ix < 0x3e300000) { 	/* |x|<2**-28 */
	        if (ix < 0x00800000)
		    return gM(0.125, gA(gM(8.0,x), gM(efx8,x)));  /*avoid underflow */
		return gA(x, gM(efx,x));
	    }
	    z = gM(x,x);
	    r = gA(pp0, gM(z,gA(pp1, gM(z,gA(pp2, gM(z,gA(pp3, gM(z,pp4))))))));
	    s = gA(one, gM(z,gA(qq1, gM(z,gA(qq2, gM(z,gA(qq3, gM(z,gA(qq4, gM(z,qq5))))))))));
	    y = gD(r,s);
	    return gA(x, gM(x,y));
	}
	if(ix < 0x3ff40000) {		/* 0.84375 <= |x| < 1.25 */
	    s = gS(fd_fabs(x),one);
	    P = gA(pa0, gM(s,gA(pa1, gM(s,gA(pa2, gM(s,gA(pa3, gM(s,gA(pa4, gM(s,gA(pa5, gM(s,pa6))))))))))));
	    Q = gA(one, gM(s,gA(qa1, gM(s,gA(qa2, gM(s,gA(qa3, gM(s,gA(qa4, gM(s,gA(qa5, gM(s,qa6))))))))))));
	    if(hx>=0) return gA(erx, gD(P,Q)); else return gS(-erx, gD(P,Q));
	}
	if (ix >= 0x40180000) {		/* inf>|x|>=6 */
	    if(hx>=0) return gS(one,tiny); else return gS(tiny,one);
	}
	x = fd_fabs(x);
 	s = gD(one,gM(x,x));
	if(ix< 0x4006DB6E) {	/* |x| < 1/0.35 */
	    r2 = gA(ra0, gM(s,gA(ra1, gM(s,gA(ra2, gM(s,gA(ra3, gM(s,gA(ra4, gM(s,gA(
				   ra5, gM(s,gA(ra6, gM(s,ra7))))))))))))));
	    S  = gA(one, gM(s,gA(sa1, gM(s,gA(sa2, gM(s,gA(sa3, gM(s,gA(sa4, gM(s,gA(
				   sa5, gM(s,gA(sa6, gM(s,gA(sa7, gM(s,sa8))))))))))))))));
	} else {	/* |x| >= 1/0.35 */
	    r2 = gA(rb0, gM(s,gA(rb1, gM(s,gA(rb2, gM(s,gA(rb3, gM(s,gA(rb4, gM(s,gA(
				   rb5, gM(s,rb6))))))))))));
	    S  = gA(one, gM(s,gA(sb1, gM(s,gA(sb2, gM(s,gA(sb3, gM(s,gA(sb4, gM(s,gA(
				   sb5, gM(s,gA(sb6, gM(s,sb7))))))))))))));
	}
	z  = x;
	FD_LO(z) = 0;
	r  =  fd_exp(gS(gM(-z,z), 0.5625)) * fd_exp(gA(gM(gS(z,x),gA(z,x)), gD(r2,S))); // TODO! NOTE! Unwrapped multiply!
	if(hx>=0) return gS(one, gD(r,x)); else return gS(gD(r,x), one);
}

double fd_erfc(double x)
{
	int hx,ix;
	double r2,S,P,Q,s,y,z,r;
	hx = FD_HI(x);
	ix = hx&0x7fffffff;
	if(ix>=0x7ff00000) {			/* fd_erfc(nan)=nan */
						/* fd_erfc(+-inf)=0,2 */
	    return (double)(((unsigned)hx>>31)<<1) + gD(one,x);
	}

	if(ix < 0x3feb0000) {		/* |x|<0.84375 */
	    if(ix < 0x3c700000)  	/* |x|<2**-56 */
		return gS(one, x);
	    z = gM(x,x);
	    r = gA(pp0, gM(z,gA(pp1, gM(z,gA(pp2, gM(z,gA(pp3, gM(z,pp4))))))));
	    s = gA(one, gM(z,gA(qq1, gM(z,gA(qq2, gM(z,gA(qq3, gM(z,gA(qq4, gM(z,qq5))))))))));
	    y = gD(r,s);
	    if(hx < 0x3fd00000) {  	/* x<1/4 */
		return gS(one, gA(x, gM(x,y)));
	    } else {
		r = gM(x,y);
		r = gA(r, gS(x, half));
	        return gS(half, r);
	    }
	}
	if(ix < 0x3ff40000) {		/* 0.84375 <= |x| < 1.25 */
	    s = gS(fd_fabs(x), one);
	    P = gA(pa0, gM(s,gA(pa1, gM(s,gA(pa2, gM(s,gA(pa3, gM(s,gA(pa4, gM(s,gA(pa5, gM(s,pa6))))))))))));
	    Q = gA(one, gM(s,gA(qa1, gM(s,gA(qa2, gM(s,gA(qa3, gM(s,gA(qa4, gM(s,gA(qa5, gM(s,qa6))))))))))));
	    if(hx>=0) {
	        z  = gS(one,erx); return gS(z, gD(P,Q));
	    } else {
		z = gA(erx, gD(P,Q)); return gA(one,z);
	    }
	}
	if (ix < 0x403c0000) {		/* |x|<28 */
	    x = fd_fabs(x);
 	    s = gD(one, gM(x,x));
	    if(ix< 0x4006DB6D) {	/* |x| < 1/.35 ~ 2.857143*/
	        r2 = gA(ra0, gM(s,gA(ra1, gM(s,gA(ra2, gM(s,gA(ra3, gM(s,gA(ra4, gM(s,gA(
				       ra5, gM(s,gA(ra6, gM(s,ra7))))))))))))));
	        S  = gA(one, gM(s,gA(sa1, gM(s,gA(sa2, gM(s,gA(sa3, gM(s,gA(sa4, gM(s,gA(
				       sa5, gM(s,gA(sa6, gM(s,gA(sa7, gM(s,sa8))))))))))))))));
	    } else {			/* |x| >= 1/.35 ~ 2.857143 */
		if(hx<0&&ix>=0x40180000) return two-tiny;/* x < -6 */
	        r2 = gA(rb0, gM(s,gA(rb1, gM(s,gA(rb2, gM(s,gA(rb3, gM(s,gA(rb4, gM(s,gA(
				       rb5, gM(s,rb6))))))))))));
	        S  = gA(one, gM(s,gA(sb1, gM(s,gA(sb2, gM(s,gA(sb3, gM(s,gA(sb4, gM(s,gA(
				       sb5, gM(s,gA(sb6, gM(s,sb7))))))))))))));
	    }
	    z  = x;
	    FD_LO(z)  = 0;
	    r  =  gA(gM(fd_exp(gS(gM(-z,z), 0.5625)), fd_exp(gM(gS(z,x),gA(z,x)))), gD(r2,S));
	    if(hx>0) return gD(r,x); else return gS(two,gD(r,x));
	} else {
	    if(hx>0) return gM(tiny,tiny); else return gS(two,tiny);
	}
}
