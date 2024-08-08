
/* @(#)w_gamma.c 1.3 95/01/18 */
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

/* double fd_gamma(double x)
 * Return the logarithm of the Gamma function of x.
 *
 * Method: call fd_gamma_r
 */

#include "fdlibm.h"
#include "fdlibm_intern.h"

extern int fd_signgam;


static const double
two52=  4.50359962737049600000e+15, /* 0x43300000, 0x00000000 */
a0  =  7.72156649015328655494e-02, /* 0x3FB3C467, 0xE37DB0C8 */
a1  =  3.22467033424113591611e-01, /* 0x3FD4A34C, 0xC4A60FAD */
a2  =  6.73523010531292681824e-02, /* 0x3FB13E00, 0x1A5562A7 */
a3  =  2.05808084325167332806e-02, /* 0x3F951322, 0xAC92547B */
a4  =  7.38555086081402883957e-03, /* 0x3F7E404F, 0xB68FEFE8 */
a5  =  2.89051383673415629091e-03, /* 0x3F67ADD8, 0xCCB7926B */
a6  =  1.19270763183362067845e-03, /* 0x3F538A94, 0x116F3F5D */
a7  =  5.10069792153511336608e-04, /* 0x3F40B6C6, 0x89B99C00 */
a8  =  2.20862790713908385557e-04, /* 0x3F2CF2EC, 0xED10E54D */
a9  =  1.08011567247583939954e-04, /* 0x3F1C5088, 0x987DFB07 */
a10 =  2.52144565451257326939e-05, /* 0x3EFA7074, 0x428CFA52 */
a11 =  4.48640949618915160150e-05, /* 0x3F07858E, 0x90A45837 */
tc  =  1.46163214496836224576e+00, /* 0x3FF762D8, 0x6356BE3F */
tf  = -1.21486290535849611461e-01, /* 0xBFBF19B9, 0xBCC38A42 */
/* tt = -(tail of tf) */
tt  = -3.63867699703950536541e-18, /* 0xBC50C7CA, 0xA48A971F */
t0  =  4.83836122723810047042e-01, /* 0x3FDEF72B, 0xC8EE38A2 */
t1  = -1.47587722994593911752e-01, /* 0xBFC2E427, 0x8DC6C509 */
t2  =  6.46249402391333854778e-02, /* 0x3FB08B42, 0x94D5419B */
t3  = -3.27885410759859649565e-02, /* 0xBFA0C9A8, 0xDF35B713 */
t4  =  1.79706750811820387126e-02, /* 0x3F9266E7, 0x970AF9EC */
t5  = -1.03142241298341437450e-02, /* 0xBF851F9F, 0xBA91EC6A */
t6  =  6.10053870246291332635e-03, /* 0x3F78FCE0, 0xE370E344 */
t7  = -3.68452016781138256760e-03, /* 0xBF6E2EFF, 0xB3E914D7 */
t8  =  2.25964780900612472250e-03, /* 0x3F6282D3, 0x2E15C915 */
t9  = -1.40346469989232843813e-03, /* 0xBF56FE8E, 0xBF2D1AF1 */
t10 =  8.81081882437654011382e-04, /* 0x3F4CDF0C, 0xEF61A8E9 */
t11 = -5.38595305356740546715e-04, /* 0xBF41A610, 0x9C73E0EC */
t12 =  3.15632070903625950361e-04, /* 0x3F34AF6D, 0x6C0EBBF7 */
t13 = -3.12754168375120860518e-04, /* 0xBF347F24, 0xECC38C38 */
t14 =  3.35529192635519073543e-04, /* 0x3F35FD3E, 0xE8C2D3F4 */
u0  = -7.72156649015328655494e-02, /* 0xBFB3C467, 0xE37DB0C8 */
u1  =  6.32827064025093366517e-01, /* 0x3FE4401E, 0x8B005DFF */
u2  =  1.45492250137234768737e+00, /* 0x3FF7475C, 0xD119BD6F */
u3  =  9.77717527963372745603e-01, /* 0x3FEF4976, 0x44EA8450 */
u4  =  2.28963728064692451092e-01, /* 0x3FCD4EAE, 0xF6010924 */
u5  =  1.33810918536787660377e-02, /* 0x3F8B678B, 0xBF2BAB09 */
v1  =  2.45597793713041134822e+00, /* 0x4003A5D7, 0xC2BD619C */
v2  =  2.12848976379893395361e+00, /* 0x40010725, 0xA42B18F5 */
v3  =  7.69285150456672783825e-01, /* 0x3FE89DFB, 0xE45050AF */
v4  =  1.04222645593369134254e-01, /* 0x3FBAAE55, 0xD6537C88 */
v5  =  3.21709242282423911810e-03, /* 0x3F6A5ABB, 0x57D0CF61 */
s0  = -7.72156649015328655494e-02, /* 0xBFB3C467, 0xE37DB0C8 */
s1  =  2.14982415960608852501e-01, /* 0x3FCB848B, 0x36E20878 */
s2  =  3.25778796408930981787e-01, /* 0x3FD4D98F, 0x4F139F59 */
s3  =  1.46350472652464452805e-01, /* 0x3FC2BB9C, 0xBEE5F2F7 */
s4  =  2.66422703033638609560e-02, /* 0x3F9B481C, 0x7E939961 */
s5  =  1.84028451407337715652e-03, /* 0x3F5E26B6, 0x7368F239 */
s6  =  3.19475326584100867617e-05, /* 0x3F00BFEC, 0xDD17E945 */
r1  =  1.39200533467621045958e+00, /* 0x3FF645A7, 0x62C4AB74 */
r2  =  7.21935547567138069525e-01, /* 0x3FE71A18, 0x93D3DCDC */
r3  =  1.71933865632803078993e-01, /* 0x3FC601ED, 0xCCFBDF27 */
r4  =  1.86459191715652901344e-02, /* 0x3F9317EA, 0x742ED475 */
r5  =  7.77942496381893596434e-04, /* 0x3F497DDA, 0xCA41A95B */
r6  =  7.32668430744625636189e-06, /* 0x3EDEBAF7, 0xA5B38140 */
w0  =  4.18938533204672725052e-01, /* 0x3FDACFE3, 0x90C97D69 */
w1  =  8.33333333333329678849e-02, /* 0x3FB55555, 0x5555553B */
w2  = -2.77777777728775536470e-03, /* 0xBF66C16C, 0x16B02E5C */
w3  =  7.93650558643019558500e-04, /* 0x3F4A019F, 0x98CF38B6 */
w4  = -5.95187557450339963135e-04, /* 0xBF4380CB, 0x8C0FE741 */
w5  =  8.36339918996282139126e-04, /* 0x3F4B67BA, 0x4CDAD5D1 */
w6  = -1.63092934096575273989e-03; /* 0xBF5AB89D, 0x0B9E43E4 */

static double sin_pi(double x)
{
	double y,z;
	int n,ix;

	ix = 0x7fffffff&FD_HI(x);

	if(ix<0x3fd00000) return _kernel_sin(gM(pi,x),zero,0);
	y = -x;		/* x is assume negative */

    /*
     * argument reduction, make sure inexact flag not raised if input
     * is an integer
     */
	z = fd_floor(y);
	if(z!=y) {				/* inexact anyway */
	    y = gM(y,0.5);
	    y = gM(2.0,gS(y, fd_floor(y)));		/* y = |x| mod 2.0 */
	    n = (int) gM(y,4.0);
	} else {
            if(ix>=0x43400000) {
                y = zero; n = 0;                 /* y must be even */
            } else {
                if(ix<0x43300000) z = gA(y,two52);	/* exact */
                n   = FD_LO(z)&1;        /* lower word of z */
                y  = n;
                n<<= 2;
            }
        }
	switch (n) {
	    case 0:   y =  _kernel_sin(gM(pi, y),zero,0); break;
	    case 1:
	    case 2:   y =  _kernel_cos(gM(pi, gS(0.5,y)),zero); break;
	    case 3:
	    case 4:   y =  _kernel_sin(gM(pi, gS(one,y)),zero,0); break;
	    case 5:
	    case 6:   y = -_kernel_cos(gM(pi, gS(y,1.5)),zero); break;
	    default:  y =  _kernel_sin(gM(pi, gS(y,2.0)),zero,0); break;
	    }
	return -y;
}

static double _ieee754_lgamma_r(double x, int *signgamp)
{
	double t,y,z,nadj,p,p1,p2,p3,q,r,w;
	int i,hx,lx,ix;

	hx = FD_HI(x);
	lx = FD_LO(x);

    /* purge off +-inf, NaN, +-0, and negative arguments */
	*signgamp = 1;
	ix = hx&0x7fffffff;
	if(ix>=0x7ff00000) return gM(x,x);
	if((ix|lx)==0) return gD(one,zero);
	if(ix<0x3b900000) {	/* |x|<2**-70, return -fd_log(|x|) */
	    if(hx<0) {
	        *signgamp = -1;
	        return -fd_log(-x);
	    } else return -fd_log(x);
	}
	if(hx<0) {
	    if(ix>=0x43300000) 	/* |x|>=2**52, must be -integer */
		return gD(one,zero);
	    t = sin_pi(x);
	    if(t==zero) return gD(one,zero); /* -integer */
	    nadj = fd_log(gD(pi, fd_fabs(gM(t, x))));
	    if(t<zero) *signgamp = -1;
	    x = -x;
	}

    /* purge off 1 and 2 */
	if((((ix-0x3ff00000)|lx)==0)||(((ix-0x40000000)|lx)==0)) r = 0;
    /* for x < 2.0 */
	else if(ix<0x40000000) {
	    if(ix<=0x3feccccc) { 	/* fd_lgamma(x) = fd_lgamma(x+1)-fd_log(x) */
		r = -fd_log(x);
		if(ix>=0x3FE76944) {y = gS(one,x); i= 0;}
		else if(ix>=0x3FCDA661) {y = gS(x, gS(tc, one)); i=1;}
	  	else {y = x; i=2;}
	    } else {
	  	r = zero;
	        if(ix>=0x3FFBB4C3) {y=gS(2.0,x);i=0;} /* [1.7316,2] */
	        else if(ix>=0x3FF3B4C4) {y=gS(x,tc);i=1;} /* [1.23,1.73] */
		else {y=gS(x,one);i=2;}
	    }
	    switch(i) {
	      case 0:
		z = gM(y,y);
		p1 =      gA(a0, gM(z,gA(a2, gM(z,gA(a4, gM(z,gA(a6, gM(z,gA(a8, gM(z,a10))))))))));
		p2 = gM(z,gA(a1, gM(z,gA(a3, gM(z,gA(a5, gM(z,gA(a7, gM(z,gA(a9, gM(z,a11)))))))))));
		p  = gA(gM(y,p1), p2);
		r  = gA(r, gS(p,gM(0.5,y))); break;
	      case 1:
		z = gM(y,y);
		w = gM(z,y);
		p1 = gA(t0, gM(w,gA(t3, gM(w,gA(t6, gM(w,gA(t9, gM(w,t12))))))));	/* parallel comp */
		p2 = gA(t1, gM(w,gA(t4, gM(w,gA(t7, gM(w,gA(t10, gM(w,t13))))))));
		p3 = gA(t2, gM(w,gA(t5, gM(w,gA(t8, gM(w,gA(t11, gM(w,t14))))))));
		p  = gS(gM(z,p1), gS(tt, gM(w,gA(p2, gM(y,p3)))));
		r  = gA(r, gA(tf, p)); break;
	      case 2:
		p1 =         gM(y,gA(u0, gM(y,gA(u1, gM(y,gA(u2, gM(y,gA(u3, gM(y,gA(u4, gM(y,u5)))))))))));
		p2 = gA(one, gM(y,gA(v1, gM(y,gA(v2, gM(y,gA(v3, gM(y,gA(v4, gM(y,v5))))))))));
		r  = gA(r, gA(gM(-0.5,y), gD(p1,p2)));
	    }
	}
	else if(ix<0x40200000) { 			/* x < 8.0 */
	    i = (int)x;
	    t = zero;
	    y = gS(x, (double)i);
	    p = gM(y,gA(s0, gM(y,gA(s1, gM(y,gA(s2, gM(y,gA(s3, gM(y,gA(s4, gM(y,gA(s5, gM(y,s6)))))))))))));
	    q = gA(one, gM(y,gA(r1, gM(y,gA(r2, gM(y,gA(r3, gM(y,gA(r4, gM(y,gA(r5, gM(y,r6))))))))))));
	    r = gA(gM(half,y), gD(p,q));
	    z = one;	/* fd_lgamma(1+s) = fd_log(s) + fd_lgamma(s) */
	    switch(i) {
	    case 7: z = gM(z, gA(y, 6.0));	/* FALLTHRU */
	    case 6: z = gM(z, gA(y, 5.0));	/* FALLTHRU */
	    case 5: z = gM(z, gA(y, 4.0));	/* FALLTHRU */
	    case 4: z = gM(z, gA(y, 3.0));	/* FALLTHRU */
	    case 3: z = gM(z, gA(y, 2.0));	/* FALLTHRU */
		    r += fd_log(z); break;
	    }
    /* 8.0 <= x < 2**58 */
	} else if (ix < 0x43900000) {
	    t = fd_log(x);
	    z = gD(one,x);
	    y = gM(z,z);
	    w = gA(w0, gM(z,gA(w1, gM(y,gA(w2, gM(y,gA(w3, gM(y,gA(w4, gM(y,gA(w5, gM(y,w6))))))))))));
	    r = gA(gM(gS(x,half),gS(t,one)), w);
	} else
    /* 2**58 <= x <= inf */
	    r =  gM(x,gS(fd_log(x), one));
	if(hx<0) r = gS(nadj, r);
	return r;
}

static double _ieee754_gamma_r(double x, int *signgamp)
{
	return _ieee754_lgamma_r(x,signgamp);
}


double fd_gamma(double x)
{
	return _ieee754_gamma_r(x,&fd_signgam);
}

double fd_gamma_r(double x, int *signgamp) /* wrapper fd_lgamma_r */
{
	return _ieee754_gamma_r(x,signgamp);
}

double fd_lgamma(double x)
{
	return _ieee754_lgamma_r(x,&fd_signgam);
}

double fd_lgamma_r(double x, int *signgamp) /* wrapper fd_lgamma_r */
{
	return _ieee754_lgamma_r(x,signgamp);
}