

/* @(#)w_pow.c 1.3 95/01/18 */
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
 * wrapper fd_pow(x,y) return x**y
 */

#include "fdlibm.h"
#include "fdlibm_intern.h"

static const double
bp[] = {1.0, 1.5,},
dp_h[] = { 0.0, 5.84962487220764160156e-01,}, /* 0x3FE2B803, 0x40000000 */
dp_l[] = { 0.0, 1.35003920212974897128e-08,}, /* 0x3E4CFDEB, 0x43CFD006 */
two53	=  9007199254740992.0,	/* 0x43400000, 0x00000000 */
	/* poly coefs for (3/2)*(fd_log(x)-2s-2/3*s**3 */
L1  =  5.99999999999994648725e-01, /* 0x3FE33333, 0x33333303 */
L2  =  4.28571428578550184252e-01, /* 0x3FDB6DB6, 0xDB6FABFF */
L3  =  3.33333329818377432918e-01, /* 0x3FD55555, 0x518F264D */
L4  =  2.72728123808534006489e-01, /* 0x3FD17460, 0xA91D4101 */
L5  =  2.30660745775561754067e-01, /* 0x3FCD864A, 0x93C9DB65 */
L6  =  2.06975017800338417784e-01, /* 0x3FCA7E28, 0x4A454EEF */
P1_   =  1.66666666666666019037e-01, /* 0x3FC55555, 0x5555553E */
P2_   = -2.77777777770155933842e-03, /* 0xBF66C16C, 0x16BEBD93 */
P3_   =  6.61375632143793436117e-05, /* 0x3F11566A, 0xAF25DE2C */
P4_   = -1.65339022054652515390e-06, /* 0xBEBBBD41, 0xC5D26BF1 */
P5_   =  4.13813679705723846039e-08, /* 0x3E663769, 0x72BEA4D0 */
lg2  =  6.93147180559945286227e-01, /* 0x3FE62E42, 0xFEFA39EF */
lg2_h  =  6.93147182464599609375e-01, /* 0x3FE62E43, 0x00000000 */
lg2_l  = -1.90465429995776804525e-09, /* 0xBE205C61, 0x0CA86C39 */
ovt =  8.0085662595372944372e-0017, /* -(1024-log2(ovfl+.5ulp)) */
cp    =  9.61796693925975554329e-01, /* 0x3FEEC709, 0xDC3A03FD =2/(3ln2) */
cp_h  =  9.61796700954437255859e-01, /* 0x3FEEC709, 0xE0000000 =(float)cp */
cp_l  = -7.02846165095275826516e-09, /* 0xBE3E2FE0, 0x145B01F5 =tail of cp_h*/
ivln2    =  1.44269504088896338700e+00, /* 0x3FF71547, 0x652B82FE =1/ln2 */
ivln2_h  =  1.44269502162933349609e+00, /* 0x3FF71547, 0x60000000 =24b 1/ln2*/
ivln2_l  =  1.92596299112661746887e-08; /* 0x3E54AE0B, 0xF85DDF44 =1/ln2 tail*/

double fd_pow(double x, double y)	/* wrapper fd_pow */
{
	double z,ax,z_h,z_l,p_h,p_l;
	double fd_y1,t1,t2,r,s,t,u,v,w;
	int i,j,k,yisint,n;
	int hx,hy,ix,iy;
	unsigned lx,ly;

	hx = FD_HI(x); lx = FD_LO(x);
	hy = FD_HI(y); ly = FD_LO(y);
	ix = hx&0x7fffffff;  iy = hy&0x7fffffff;

    /* y==zero: x**0 = 1 */
	if((iy|ly)==0) return one;

    /* +-NaN return x+y */
	if(ix > 0x7ff00000 || ((ix==0x7ff00000)&&(lx!=0)) ||
	   iy > 0x7ff00000 || ((iy==0x7ff00000)&&(ly!=0)))
		return gA(x,y);

    /* determine if y is an odd int when x < 0
     * yisint = 0	... y is not an integer
     * yisint = 1	... y is an odd int
     * yisint = 2	... y is an even int
     */
	yisint  = 0;
	if(hx<0) {
	    if(iy>=0x43400000) yisint = 2; /* even integer y */
	    else if(iy>=0x3ff00000) {
		k = (iy>>20)-0x3ff;	   /* exponent */
		if(k>20) {
		    j = ly>>(52-k);
		    if((j<<(52-k))==ly) yisint = 2-(j&1);
		} else if(ly==0) {
		    j = iy>>(20-k);
		    if((j<<(20-k))==iy) yisint = 2-(j&1);
		}
	    }
	}

    /* special value of y */
	if(ly==0) {
	    if (iy==0x7ff00000) {	/* y is +-inf */
	        if(((ix-0x3ff00000)|lx)==0)
		    return gS(y, y);	/* inf**+-1 is NaN */
	        else if (ix >= 0x3ff00000)/* (|x|>1)**+-inf = inf,0 */
		    return (hy>=0)? y: zero;
	        else			/* (|x|<1)**-,+inf = inf,0 */
		    return (hy<0)?-y: zero;
	    }
	    if(iy==0x3ff00000) {	/* y is  +-1 */
		if(hy<0) return gD(one,x); else return x;
	    }
	    if(hy==0x40000000) return gM(x,x); /* y is  2 */
	    if(hy==0x3fe00000) {	/* y is  0.5 */
		if(hx>=0)	/* x >= +0 */
		return gSqrt(x);
	    }
	}

	ax   = fd_fabs(x);
    /* special value of x */
	if(lx==0) {
	    if(ix==0x7ff00000||ix==0||ix==0x3ff00000){
		z = ax;			/*x is +-0,+-inf,+-1*/
		if(hy<0) z = gD(one,z);	/* z = (1/|x|) */
		if(hx<0) {
		    if(((ix-0x3ff00000)|yisint)==0) {
			z = gD(gS(z,z), gS(z,z)); /* (-1)**non-int is NaN */
		    } else if(yisint==1)
			z = -z;		/* (x<0)**odd = -(|x|**odd) */
		}
		return z;
	    }
	}

    /* (x<0)**(non-int) is NaN */
	if((((hx>>31)+1)|yisint)==0) return gD(gS(x,x),gS(x,x));

    /* |y| is huge */
	if(iy>0x41e00000) { /* if |y| > 2**31 */
	    if(iy>0x43f00000){	/* if |y| > 2**64, must o/uflow */
		if(ix<=0x3fefffff) return (hy<0)? gM(huge,huge):gM(tiny,tiny);
		if(ix>=0x3ff00000) return (hy>0)? gM(huge,huge):gM(tiny,tiny);
	    }
	/* over/underflow if x is not close to one */
	    if(ix<0x3fefffff) return (hy<0)? gM(huge,huge):gM(tiny,tiny);
	    if(ix>0x3ff00000) return (hy>0)? gM(huge,huge):gM(tiny,tiny);
	/* now |1-x| is tiny <= 2**-20, suffice to compute
	   fd_log(x) by x-x^2/2+x^3/3-x^4/4 */
	    t = gS(x,1);		/* t has 20 trailing zeros */
	    w = gM(gM(t,t),gS(0.5, gM(t,gS(0.3333333333333333333333, gM(t,0.25)))));
	    u = gM(ivln2_h,t);	/* ivln2_h has 21 sig. bits */
	    v = gS(gM(t,ivln2_l), gM(w,ivln2));
	    t1 = gA(u,v);
	    FD_LO(t1) = 0;
	    t2 = gS(v,gS(t1,u));
	} else {
	    double s2,s_h,s_l,t_h,t_l;
	    n = 0;
	/* take care subnormal number */
	    if(ix<0x00100000)
		{ax = gM(ax,two53); n -= 53; ix = FD_HI(ax); }
	    n  += ((ix)>>20)-0x3ff;
	    j  = ix&0x000fffff;
	/* determine interval */
	    ix = j|0x3ff00000;		/* normalize ix */
	    if(j<=0x3988E) k=0;		/* |x|<fd_sqrt(3/2) */
	    else if(j<0xBB67A) k=1;	/* |x|<fd_sqrt(3)   */
	    else {k=0;n+=1;ix -= 0x00100000;}
	    FD_HI(ax) = ix;

	/* compute s = s_h+s_l = (x-1)/(x+1) or (x-1.5)/(x+1.5) */
	    u = gS(ax,bp[k]);		/* bp[0]=1.0, bp[1]=1.5 */
	    v = gD(one,gA(ax, bp[k]));
	    s = gM(u,v);
	    s_h = s;
	    FD_LO(s_h) = 0;
	/* t_h=ax+bp[k] High */
	    t_h = zero;
	    FD_HI(t_h)=((ix>>1)|0x20000000)+0x00080000+(k<<18);
	    t_l = gS(ax, gS(t_h, bp[k]));
	    s_l = gM(v,gS(gS(u, gM(s_h,t_h)), gM(s_h,t_l)));
	/* compute fd_log(ax) */
	    s2 = gM(s,s);
	    r = gM(gM(s2,s2), gA(L1, gM(s2,gA(L2, gM(s2,gA(L3, gM(s2,gA(L4, gM(s2,gA(L5, gM(s2,L6)))))))))));
	    r = gA(r, gM(s_l,gA(s_h,s)));
	    s2  = gM(s_h,s_h);
	    t_h = gA(gA(3.0,s2),r);
	    FD_LO(t_h) = 0;
	    t_l = gS(r,gS(gS(t_h,3.0), s2));
	/* u+v = s*(1+...) */
	    u = gM(s_h,t_h);
	    v = gA(gM(s_l,t_h), gM(t_l,s));
	/* 2/(3log2)*(s+...) */
	    p_h = gA(u,v);
	    FD_LO(p_h) = 0;
	    p_l = gS(v,gS(p_h,u));
	    z_h = gM(cp_h,p_h);		/* cp_h+cp_l = 2/(3*log2) */
	    z_l = gA(gA(gM(cp_l,p_h), gM(p_l,cp)), dp_l[k]);
	/* log2(ax) = (s+..)*2/(3*log2) = n + dp_h + z_h + z_l */
	    t = (double)n;
	    t1 = gA(gA(gA(z_h, z_l), dp_h[k]), t);
	    FD_LO(t1) = 0;
	    t2 = gS(z_l, gS(gS(gS(t1,t), dp_h[k]), z_h));
	}

	s = one; /* s (sign of result -ve**odd) = -1 else = 1 */
	if((((hx>>31)+1)|(yisint-1))==0) s = -one;/* (-ve)**(odd int) */

    /* split up y into fd_y1+y2 and compute (fd_y1+y2)*(t1+t2) */
	fd_y1  = y;
	FD_LO(fd_y1) = 0;
	p_l = gA(gM(gS(y,fd_y1),t1), gM(y,t2));
	p_h = gM(fd_y1,t1);
	z = gA(p_l,p_h);
	j = FD_HI(z);
	i = FD_LO(z);
	if (j>=0x40900000) {				/* z >= 1024 */
	    if(((j-0x40900000)|i)!=0)			/* if z > 1024 */
		return gM(gM(s,huge),huge);			/* overflow */
	    else {
		if(gA(p_l, ovt) > gS(z, p_h)) return gM(gM(s,huge),huge);	/* overflow */
	    }
	} else if((j&0x7fffffff)>=0x4090cc00 ) {	/* z <= -1075 */
	    if(((j-0xc090cc00)|i)!=0) 		/* z < -1075 */
		return gM(gM(s,tiny),tiny);		/* underflow */
	    else {
		if(p_l <= gS(z,p_h)) return gM(gM(s,tiny),tiny);	/* underflow */
	    }
	}
    /*
     * compute 2**(p_h+p_l)
     */
	i = j&0x7fffffff;
	k = (i>>20)-0x3ff;
	n = 0;
	if(i>0x3fe00000) {		/* if |z| > 0.5, set n = [z+0.5] */
	    n = j+(0x00100000>>(k+1));
	    k = ((n&0x7fffffff)>>20)-0x3ff;	/* new k for n */
	    t = zero;
	    FD_HI(t) = (n&~(0x000fffff>>k));
	    n = ((n&0x000fffff)|0x00100000)>>(20-k);
	    if(j<0) n = -n;
	    p_h = gS(p_h, t);
	}
	t = gA(p_l,p_h);
	FD_LO(t) = 0;
	u = gM(t,lg2_h);
	v = gA(gM(gS(p_l,gS(t,p_h)), lg2), gM(t,lg2_l));
	z = gA(u,v);
	w = gS(v,gS(z,u));
	t  = gM(z,z);
	t1 = gS(z, gM(t,gA(P1_, gM(t,gA(P2_, gM(t,gA(P3_, gM(t,gA(P4_, gM(t,P5_))))))))));
	r  = gS(gD(gM(z,t1), gS(t1, two)), gA(w, gM(z,w)));
	z  = gS(one, gS(r, z));
	j  = FD_HI(z);
	j += (n<<20);
	if((j>>20)<=0) z = fd_scalbn(z,n);	/* subnormal output */
	else FD_HI(z) += (n<<20);
	return gM(s,z);
}
