#ifndef UUID_0DFC0705C06145FD593785907FBDDF54
#define UUID_0DFC0705C06145FD593785907FBDDF54

#include "../platform.h"
#include "../config.h"


/* @(#)fdlibm.h 1.5 95/01/18 */
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

#ifdef __cplusplus
extern "C" {
#endif

#undef FD_IEEE_LIBM
#define FD_IEEE_LIBM 1

#if 1
#if TL_LITTLE_ENDIAN
#define FD_HI(x) (*(1+(int*)&(x)))
#define FD_LO(x) (*(int*)&(x))
#define FD_HIp(x) (*(1+(int*)(x)))
#define FD_LOp(x) (*(int*)(x))
#else
#define FD_HI(x) (*(int*)&(x))
#define FD_LO(x) (*(1+(int*)&(x)))
#define FD_HIp(x) (*(int*)(x))
#define FD_LOp(x) (*(1+(int*)(x)))
#endif

#else
/* These cannot be assigned to, thus cannot be used at the moment.
** It would be great to fix this so that we don't violate aliasing
** rules with the above. */

typedef union fd_double_int_
{
	struct i_
	{
		int first, second;
	} i;

	double d;
} fd_double_int;

TL_INLINE int FD_HIp(double* x)
{
	fd_double_int u;
	u.d = *x;
#if TL_LITTLE_ENDIAN
	return u.i.second;
#else
	return u.i.first;
#endif
}

TL_INLINE int FD_LOp(double* x)
{
	fd_double_int u;
	u.d = *x;
#if TL_LITTLE_ENDIAN
	return u.i.first;
#else
	return u.i.second;
#endif
}

#define FD_HI(x) FD_HIp(&(x))
#define FD_LO(x) FD_LOp(&(x))

#endif

/*
 * ANSI/POSIX
 */

extern int fd_signgam;

#define	FD_MAXFLOAT	((float)3.40282346638528860e+38)

enum fdversion {fdlibm_ieee = -1, fdlibm_svid, fdlibm_xopen, fdlibm_posix};

#define FD_LIB_VERSION_TYPE enum fdversion
#define FD_LIB_VERSION _fdlib_version

/* if global variable _LIB_VERSION is not desirable, one may
 * change the following to be a constant by:
 *	#define _LIB_VERSION_TYPE const enum version
 * In that case, after one initializes the value _LIB_VERSION (see
 * s_lib_version.c) during compile time, it cannot be modified
 * in the middle of a program
 */
extern  FD_LIB_VERSION_TYPE  FD_LIB_VERSION;

#define FD_IEEE_  fdlibm_ieee
#define FD_SVID_  fdlibm_svid
#define FD_XOPEN_ fdlibm_xopen
#define FD_POSIX_ fdlibm_posix

struct fd_exception {
	int type;
	char *name;
	double arg1;
	double arg2;
	double retval;
};

#define	FD_HUGE		FD_MAXFLOAT

/*
 * set FD_X_TLOSS = pi*2**52, which is possibly defined in <values.h>
 * (one may replace the following line by "#include <values.h>")
 */

#define FD_X_TLOSS		1.41484755040568800000e+16

#define	FD_DOMAIN		1
#define	FD_SING		2
#define	FD_OVERFLOW	3
#define	FD_UNDERFLOW	4
#define	FD_TLOSS		5
#define	FD_PLOSS		6

/*
 * ANSI/POSIX
 */
FDLIBM_API double fd_acos (double);
FDLIBM_API double fd_asin (double);
FDLIBM_API double fd_atan (double);
FDLIBM_API double fd_atan2 (double, double);
FDLIBM_API double fd_cos (double);
FDLIBM_API double fd_sin (double);
FDLIBM_API double fd_tan (double);

FDLIBMH_API double fd_cosh (double);
FDLIBMH_API double fd_sinh (double);
FDLIBMH_API double fd_tanh (double);

FDLIBM_API double fd_exp (double);
FDLIBM_API double fd_frexp (double, int *);
FDLIBM_API double fd_ldexp (double, int);
FDLIBM_API double fd_log (double);
FDLIBM_API double fd_log10 (double);
FDLIBM_API double fd_modf (double, double *);

FDLIBM_API double fd_pow (double, double);
FDLIBM_API double fd_sqrt (double);

FDLIBM_API double fd_ceil (double);
FDLIBM_API double fd_fabs (double);
FDLIBM_API double fd_floor (double);
FDLIBM_API double fd_fmod (double, double);
FDLIBM_API int fd_isnan (double);
FDLIBM_API int fd_finite (double);

FDLIBM2_API double fd_hypot (double, double);
FDLIBM2_API double fd_erf (double);
FDLIBM2_API double fd_erfc (double);
FDLIBM2_API double fd_gamma (double);
FDLIBM2_API double fd_j0 (double);
FDLIBM2_API double fd_j1 (double);
FDLIBM2_API double fd_jn (int, double);
FDLIBM2_API double fd_lgamma (double);
FDLIBM2_API double fd_y0 (double);
FDLIBM2_API double fd_y1 (double);
FDLIBM2_API double fd_yn (int, double);

FDLIBMH_API double fd_acosh (double);
FDLIBMH_API double fd_asinh (double);
FDLIBMH_API double fd_atanh (double);
FDLIBM_API double fd_cbrt (double);
FDLIBM_API double fd_logb (double);
FDLIBM_API double fd_nextafter (double, double);
FDLIBM_API double fd_remainder (double, double);
#ifdef _SCALB_INT
FDLIBM_API double fd_scalb (double, int);
#else
FDLIBM_API double fd_scalb (double, double);
#endif

FDLIBM_API int fd_matherr (struct fd_exception *);

/*
 * IEEE Test Vector
 */
FDLIBM_API double fd_significand (double);

/*
 * Functions callable from C, intended to support IEEE arithmetic.
 */
FDLIBM_API double fd_copysign (double, double);
FDLIBM_API int fd_ilogb (double);
FDLIBM_API double fd_rint (double);
FDLIBM_API double fd_scalbn (double, int);

/*
 * BSD math library entry points
 */
FDLIBM2_API double fd_expm1 (double);
FDLIBM2_API double fd_log1p (double);

/*
 * Reentrant version of fd_gamma & fd_lgamma; passes fd_signgam back by reference
 * as the second argument; user must allocate space for fd_signgam.
 */
#ifdef _REENTRANT
FDLIBM2_API double fd_gamma_r (double, int *);
FDLIBM2_API double fd_lgamma_r (double, int *);
#endif	/* _REENTRANT */

/* ieee style elementary functions */

FDLIBM_INTERNAL int    _ieee754_rem_pio2 (double,double*);

/* fdlibm kernel function */
FDLIBM_INTERNAL double _kernel_standard (double,double,int);
FDLIBM_INTERNAL double _kernel_sin (double,double,int);
FDLIBM_INTERNAL double _kernel_cos (double,double);
FDLIBM_INTERNAL double _kernel_tan (double,double,int);
FDLIBM_INTERNAL int    _kernel_rem_pio2 (double*,double*,int,int,int,const int*);

#ifdef __cplusplus
}
#endif

#endif // UUID_0DFC0705C06145FD593785907FBDDF54
