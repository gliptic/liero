
/* @(#)k_standard.c 1.3 95/01/18 */
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

#include "fdlibm.h"
#include "fdlibm_intern.h"
#include <errno.h>

#ifndef _USE_WRITE
#include <stdio.h>			/* fputs(), stderr */
#define	WRITE2(u,v)	fputs(u, stderr)
#else	/* !defined(_USE_WRITE) */
#include <unistd.h>			/* write */
#define	WRITE2(u,v)	write(2, u, v)
#undef fflush
#endif	/* !defined(_USE_WRITE) */

/*
 * Standard conformance (non-IEEE) on fd_exception cases.
 * Mapping:
 *	1 -- fd_acos(|x|>1)
 *	2 -- fd_asin(|x|>1)
 *	3 -- fd_atan2(+-0,+-0)
 *	4 -- fd_hypot overflow
 *	5 -- fd_cosh overflow
 *	6 -- fd_exp overflow
 *	7 -- fd_exp underflow
 *	8 -- fd_y0(0)
 *	9 -- fd_y0(-ve)
 *	10-- fd_y1(0)
 *	11-- fd_y1(-ve)
 *	12-- fd_yn(0)
 *	13-- fd_yn(-ve)
 *	14-- fd_lgamma(fd_finite) overflow
 *	15-- fd_lgamma(-integer)
 *	16-- fd_log(0)
 *	17-- fd_log(x<0)
 *	18-- fd_log10(0)
 *	19-- fd_log10(x<0)
 *	20-- fd_pow(0.0,0.0)
 *	21-- fd_pow(x,y) overflow
 *	22-- fd_pow(x,y) underflow
 *	23-- fd_pow(0,negative)
 *	24-- fd_pow(neg,non-integral)
 *	25-- fd_sinh(fd_finite) overflow
 *	26-- fd_sqrt(negative)
 *      27-- fd_fmod(x,0)
 *      28-- fd_remainder(x,0)
 *	29-- fd_acosh(x<1)
 *	30-- fd_atanh(|x|>1)
 *	31-- fd_atanh(|x|=1)
 *	32-- fd_scalb overflow
 *	33-- fd_scalb underflow
 *	34-- fd_j0(|x|>FD_X_TLOSS)
 *	35-- fd_y0(x>FD_X_TLOSS)
 *	36-- fd_j1(|x|>FD_X_TLOSS)
 *	37-- fd_y1(x>FD_X_TLOSS)
 *	38-- fd_jn(|x|>FD_X_TLOSS, n)
 *	39-- fd_yn(x>FD_X_TLOSS, n)
 *	40-- fd_gamma(fd_finite) overflow
 *	41-- fd_gamma(-integer)
 *	42-- fd_pow(NaN,0.0)
 */

FDLIBM_INTERNAL double _kernel_standard(double x, double y, int type)
{
	struct fd_exception exc;
#ifndef HUGE_VAL	/* this is the only routine that uses HUGE_VAL */
#define HUGE_VAL inf
	double inf = 0.0;

	FD_HI(inf) = 0x7ff00000;	/* set inf to infinite */
#endif

#ifdef _USE_WRITE
	(void) fflush(stdout);
#endif
	exc.arg1 = x;
	exc.arg2 = y;
	switch(type) {
	    case 1:
		/* fd_acos(|x|>1) */
		exc.type = FD_DOMAIN;
		exc.name = "fd_acos";
		exc.retval = zero;
		if (FD_LIB_VERSION == FD_POSIX_)
		  errno = EDOM;
		else if (!fd_matherr(&exc)) {
		  if(FD_LIB_VERSION == FD_SVID_) {
		    (void) WRITE2("fd_acos: FD_DOMAIN error\n", 19);
		  }
		  errno = EDOM;
		}
		break;
	    case 2:
		/* fd_asin(|x|>1) */
		exc.type = FD_DOMAIN;
		exc.name = "fd_asin";
		exc.retval = zero;
		if(FD_LIB_VERSION == FD_POSIX_)
		  errno = EDOM;
		else if (!fd_matherr(&exc)) {
		  if(FD_LIB_VERSION == FD_SVID_) {
		    	(void) WRITE2("fd_asin: FD_DOMAIN error\n", 19);
		  }
		  errno = EDOM;
		}
		break;
	    case 3:
		/* fd_atan2(+-0,+-0) */
		exc.arg1 = y;
		exc.arg2 = x;
		exc.type = FD_DOMAIN;
		exc.name = "fd_atan2";
		exc.retval = zero;
		if(FD_LIB_VERSION == FD_POSIX_)
		  errno = EDOM;
		else if (!fd_matherr(&exc)) {
		  if(FD_LIB_VERSION == FD_SVID_) {
			(void) WRITE2("fd_atan2: FD_DOMAIN error\n", 20);
		      }
		  errno = EDOM;
		}
		break;
	    case 4:
		/* fd_hypot(fd_finite,fd_finite) overflow */
		exc.type = FD_OVERFLOW;
		exc.name = "fd_hypot";
		if (FD_LIB_VERSION == FD_SVID_)
		  exc.retval = FD_HUGE;
		else
		  exc.retval = HUGE_VAL;
		if (FD_LIB_VERSION == FD_POSIX_)
		  errno = ERANGE;
		else if (!fd_matherr(&exc)) {
			errno = ERANGE;
		}
		break;
	    case 5:
		/* fd_cosh(fd_finite) overflow */
		exc.type = FD_OVERFLOW;
		exc.name = "fd_cosh";
		if (FD_LIB_VERSION == FD_SVID_)
		  exc.retval = FD_HUGE;
		else
		  exc.retval = HUGE_VAL;
		if (FD_LIB_VERSION == FD_POSIX_)
		  errno = ERANGE;
		else if (!fd_matherr(&exc)) {
			errno = ERANGE;
		}
		break;
	    case 6:
		/* fd_exp(fd_finite) overflow */
		exc.type = FD_OVERFLOW;
		exc.name = "fd_exp";
		if (FD_LIB_VERSION == FD_SVID_)
		  exc.retval = FD_HUGE;
		else
		  exc.retval = HUGE_VAL;
		if (FD_LIB_VERSION == FD_POSIX_)
		  errno = ERANGE;
		else if (!fd_matherr(&exc)) {
			errno = ERANGE;
		}
		break;
	    case 7:
		/* fd_exp(fd_finite) underflow */
		exc.type = FD_UNDERFLOW;
		exc.name = "fd_exp";
		exc.retval = zero;
		if (FD_LIB_VERSION == FD_POSIX_)
		  errno = ERANGE;
		else if (!fd_matherr(&exc)) {
			errno = ERANGE;
		}
		break;
	    case 8:
		/* fd_y0(0) = -inf */
		exc.type = FD_DOMAIN;	/* should be FD_SING for IEEE */
		exc.name = "fd_y0";
		if (FD_LIB_VERSION == FD_SVID_)
		  exc.retval = -FD_HUGE;
		else
		  exc.retval = -HUGE_VAL;
		if (FD_LIB_VERSION == FD_POSIX_)
		  errno = EDOM;
		else if (!fd_matherr(&exc)) {
		  if (FD_LIB_VERSION == FD_SVID_) {
			(void) WRITE2("fd_y0: FD_DOMAIN error\n", 17);
		      }
		  errno = EDOM;
		}
		break;
	    case 9:
		/* fd_y0(x<0) = NaN */
		exc.type = FD_DOMAIN;
		exc.name = "fd_y0";
		if (FD_LIB_VERSION == FD_SVID_)
		  exc.retval = -FD_HUGE;
		else
		  exc.retval = -HUGE_VAL;
		if (FD_LIB_VERSION == FD_POSIX_)
		  errno = EDOM;
		else if (!fd_matherr(&exc)) {
		  if (FD_LIB_VERSION == FD_SVID_) {
			(void) WRITE2("fd_y0: FD_DOMAIN error\n", 17);
		      }
		  errno = EDOM;
		}
		break;
	    case 10:
		/* fd_y1(0) = -inf */
		exc.type = FD_DOMAIN;	/* should be FD_SING for IEEE */
		exc.name = "fd_y1";
		if (FD_LIB_VERSION == FD_SVID_)
		  exc.retval = -FD_HUGE;
		else
		  exc.retval = -HUGE_VAL;
		if (FD_LIB_VERSION == FD_POSIX_)
		  errno = EDOM;
		else if (!fd_matherr(&exc)) {
		  if (FD_LIB_VERSION == FD_SVID_) {
			(void) WRITE2("fd_y1: FD_DOMAIN error\n", 17);
		      }
		  errno = EDOM;
		}
		break;
	    case 11:
		/* fd_y1(x<0) = NaN */
		exc.type = FD_DOMAIN;
		exc.name = "fd_y1";
		if (FD_LIB_VERSION == FD_SVID_)
		  exc.retval = -FD_HUGE;
		else
		  exc.retval = -HUGE_VAL;
		if (FD_LIB_VERSION == FD_POSIX_)
		  errno = EDOM;
		else if (!fd_matherr(&exc)) {
		  if (FD_LIB_VERSION == FD_SVID_) {
			(void) WRITE2("fd_y1: FD_DOMAIN error\n", 17);
		      }
		  errno = EDOM;
		}
		break;
	    case 12:
		/* fd_yn(n,0) = -inf */
		exc.type = FD_DOMAIN;	/* should be FD_SING for IEEE */
		exc.name = "fd_yn";
		if (FD_LIB_VERSION == FD_SVID_)
		  exc.retval = -FD_HUGE;
		else
		  exc.retval = -HUGE_VAL;
		if (FD_LIB_VERSION == FD_POSIX_)
		  errno = EDOM;
		else if (!fd_matherr(&exc)) {
		  if (FD_LIB_VERSION == FD_SVID_) {
			(void) WRITE2("fd_yn: FD_DOMAIN error\n", 17);
		      }
		  errno = EDOM;
		}
		break;
	    case 13:
		/* fd_yn(x<0) = NaN */
		exc.type = FD_DOMAIN;
		exc.name = "fd_yn";
		if (FD_LIB_VERSION == FD_SVID_)
		  exc.retval = -FD_HUGE;
		else
		  exc.retval = -HUGE_VAL;
		if (FD_LIB_VERSION == FD_POSIX_)
		  errno = EDOM;
		else if (!fd_matherr(&exc)) {
		  if (FD_LIB_VERSION == FD_SVID_) {
			(void) WRITE2("fd_yn: FD_DOMAIN error\n", 17);
		      }
		  errno = EDOM;
		}
		break;
	    case 14:
		/* fd_lgamma(fd_finite) overflow */
		exc.type = FD_OVERFLOW;
		exc.name = "fd_lgamma";
                if (FD_LIB_VERSION == FD_SVID_)
                  exc.retval = FD_HUGE;
                else
                  exc.retval = HUGE_VAL;
                if (FD_LIB_VERSION == FD_POSIX_)
			errno = ERANGE;
                else if (!fd_matherr(&exc)) {
                        errno = ERANGE;
		}
		break;
	    case 15:
		/* fd_lgamma(-integer) or fd_lgamma(0) */
		exc.type = FD_SING;
		exc.name = "fd_lgamma";
                if (FD_LIB_VERSION == FD_SVID_)
                  exc.retval = FD_HUGE;
                else
                  exc.retval = HUGE_VAL;
		if (FD_LIB_VERSION == FD_POSIX_)
		  errno = EDOM;
		else if (!fd_matherr(&exc)) {
		  if (FD_LIB_VERSION == FD_SVID_) {
			(void) WRITE2("fd_lgamma: FD_SING error\n", 19);
		      }
		  errno = EDOM;
		}
		break;
	    case 16:
		/* fd_log(0) */
		exc.type = FD_SING;
		exc.name = "fd_log";
		if (FD_LIB_VERSION == FD_SVID_)
		  exc.retval = -FD_HUGE;
		else
		  exc.retval = -HUGE_VAL;
		if (FD_LIB_VERSION == FD_POSIX_)
		  errno = ERANGE;
		else if (!fd_matherr(&exc)) {
		  if (FD_LIB_VERSION == FD_SVID_) {
			(void) WRITE2("fd_log: FD_SING error\n", 16);
		      }
		  errno = EDOM;
		}
		break;
	    case 17:
		/* fd_log(x<0) */
		exc.type = FD_DOMAIN;
		exc.name = "fd_log";
		if (FD_LIB_VERSION == FD_SVID_)
		  exc.retval = -FD_HUGE;
		else
		  exc.retval = -HUGE_VAL;
		if (FD_LIB_VERSION == FD_POSIX_)
		  errno = EDOM;
		else if (!fd_matherr(&exc)) {
		  if (FD_LIB_VERSION == FD_SVID_) {
			(void) WRITE2("fd_log: FD_DOMAIN error\n", 18);
		      }
		  errno = EDOM;
		}
		break;
	    case 18:
		/* fd_log10(0) */
		exc.type = FD_SING;
		exc.name = "fd_log10";
		if (FD_LIB_VERSION == FD_SVID_)
		  exc.retval = -FD_HUGE;
		else
		  exc.retval = -HUGE_VAL;
		if (FD_LIB_VERSION == FD_POSIX_)
		  errno = ERANGE;
		else if (!fd_matherr(&exc)) {
		  if (FD_LIB_VERSION == FD_SVID_) {
			(void) WRITE2("fd_log10: FD_SING error\n", 18);
		      }
		  errno = EDOM;
		}
		break;
	    case 19:
		/* fd_log10(x<0) */
		exc.type = FD_DOMAIN;
		exc.name = "fd_log10";
		if (FD_LIB_VERSION == FD_SVID_)
		  exc.retval = -FD_HUGE;
		else
		  exc.retval = -HUGE_VAL;
		if (FD_LIB_VERSION == FD_POSIX_)
		  errno = EDOM;
		else if (!fd_matherr(&exc)) {
		  if (FD_LIB_VERSION == FD_SVID_) {
			(void) WRITE2("fd_log10: FD_DOMAIN error\n", 20);
		      }
		  errno = EDOM;
		}
		break;
	    case 20:
		/* fd_pow(0.0,0.0) */
		/* error only if FD_LIB_VERSION == FD_SVID_ */
		exc.type = FD_DOMAIN;
		exc.name = "fd_pow";
		exc.retval = zero;
		if (FD_LIB_VERSION != FD_SVID_) exc.retval = 1.0;
		else if (!fd_matherr(&exc)) {
			(void) WRITE2("fd_pow(0,0): FD_DOMAIN error\n", 23);
			errno = EDOM;
		}
		break;
	    case 21:
		/* fd_pow(x,y) overflow */
		exc.type = FD_OVERFLOW;
		exc.name = "fd_pow";
		if (FD_LIB_VERSION == FD_SVID_) {
		  exc.retval = FD_HUGE;
		  y = gM(y, 0.5);
		  if(x<zero&&fd_rint(y)!=y) exc.retval = -FD_HUGE;
		} else {
		  exc.retval = HUGE_VAL;
		  y = gM(y, 0.5);
		  if(x<zero&&fd_rint(y)!=y) exc.retval = -HUGE_VAL;
		}
		if (FD_LIB_VERSION == FD_POSIX_)
		  errno = ERANGE;
		else if (!fd_matherr(&exc)) {
			errno = ERANGE;
		}
		break;
	    case 22:
		/* fd_pow(x,y) underflow */
		exc.type = FD_UNDERFLOW;
		exc.name = "fd_pow";
		exc.retval =  zero;
		if (FD_LIB_VERSION == FD_POSIX_)
		  errno = ERANGE;
		else if (!fd_matherr(&exc)) {
			errno = ERANGE;
		}
		break;
	    case 23:
		/* 0**neg */
		exc.type = FD_DOMAIN;
		exc.name = "fd_pow";
		if (FD_LIB_VERSION == FD_SVID_)
		  exc.retval = zero;
		else
		  exc.retval = -HUGE_VAL;
		if (FD_LIB_VERSION == FD_POSIX_)
		  errno = EDOM;
		else if (!fd_matherr(&exc)) {
		  if (FD_LIB_VERSION == FD_SVID_) {
			(void) WRITE2("fd_pow(0,neg): FD_DOMAIN error\n", 25);
		      }
		  errno = EDOM;
		}
		break;
	    case 24:
		/* neg**non-integral */
		exc.type = FD_DOMAIN;
		exc.name = "fd_pow";
		if (FD_LIB_VERSION == FD_SVID_)
		    exc.retval = zero;
		else
		    exc.retval = zero/zero;	/* X/Open allow NaN */
		if (FD_LIB_VERSION == FD_POSIX_)
		   errno = EDOM;
		else if (!fd_matherr(&exc)) {
		  if (FD_LIB_VERSION == FD_SVID_) {
			(void) WRITE2("neg**non-integral: FD_DOMAIN error\n", 32);
		      }
		  errno = EDOM;
		}
		break;
	    case 25:
		/* fd_sinh(fd_finite) overflow */
		exc.type = FD_OVERFLOW;
		exc.name = "fd_sinh";
		if (FD_LIB_VERSION == FD_SVID_)
		  exc.retval = ( (x>zero) ? FD_HUGE : -FD_HUGE);
		else
		  exc.retval = ( (x>zero) ? HUGE_VAL : -HUGE_VAL);
		if (FD_LIB_VERSION == FD_POSIX_)
		  errno = ERANGE;
		else if (!fd_matherr(&exc)) {
			errno = ERANGE;
		}
		break;
	    case 26:
		/* fd_sqrt(x<0) */
		exc.type = FD_DOMAIN;
		exc.name = "fd_sqrt";
		if (FD_LIB_VERSION == FD_SVID_)
		  exc.retval = zero;
		else
		  exc.retval = zero/zero;
		if (FD_LIB_VERSION == FD_POSIX_)
		  errno = EDOM;
		else if (!fd_matherr(&exc)) {
		  if (FD_LIB_VERSION == FD_SVID_) {
			(void) WRITE2("fd_sqrt: FD_DOMAIN error\n", 19);
		      }
		  errno = EDOM;
		}
		break;
            case 27:
                /* fd_fmod(x,0) */
                exc.type = FD_DOMAIN;
                exc.name = "fd_fmod";
                if (FD_LIB_VERSION == FD_SVID_)
                    exc.retval = x;
		else
		    exc.retval = zero/zero;
                if (FD_LIB_VERSION == FD_POSIX_)
                  errno = EDOM;
                else if (!fd_matherr(&exc)) {
                  if (FD_LIB_VERSION == FD_SVID_) {
                    (void) WRITE2("fd_fmod:  FD_DOMAIN error\n", 20);
                  }
                  errno = EDOM;
                }
                break;
            case 28:
                /* fd_remainder(x,0) */
                exc.type = FD_DOMAIN;
                exc.name = "fd_remainder";
                exc.retval = gD(zero,zero);
                if (FD_LIB_VERSION == FD_POSIX_)
                  errno = EDOM;
                else if (!fd_matherr(&exc)) {
                  if (FD_LIB_VERSION == FD_SVID_) {
                    (void) WRITE2("fd_remainder: FD_DOMAIN error\n", 24);
                  }
                  errno = EDOM;
                }
                break;
            case 29:
                /* fd_acosh(x<1) */
                exc.type = FD_DOMAIN;
                exc.name = "fd_acosh";
                exc.retval = gD(zero,zero);
                if (FD_LIB_VERSION == FD_POSIX_)
                  errno = EDOM;
                else if (!fd_matherr(&exc)) {
                  if (FD_LIB_VERSION == FD_SVID_) {
                    (void) WRITE2("fd_acosh: FD_DOMAIN error\n", 20);
                  }
                  errno = EDOM;
                }
                break;
            case 30:
                /* fd_atanh(|x|>1) */
                exc.type = FD_DOMAIN;
                exc.name = "fd_atanh";
                exc.retval = gD(zero,zero);
                if (FD_LIB_VERSION == FD_POSIX_)
                  errno = EDOM;
                else if (!fd_matherr(&exc)) {
                  if (FD_LIB_VERSION == FD_SVID_) {
                    (void) WRITE2("fd_atanh: FD_DOMAIN error\n", 20);
                  }
                  errno = EDOM;
                }
                break;
            case 31:
                /* fd_atanh(|x|=1) */
                exc.type = FD_SING;
                exc.name = "fd_atanh";
		exc.retval = x/zero;	/* sign(x)*inf */
                if (FD_LIB_VERSION == FD_POSIX_)
                  errno = EDOM;
                else if (!fd_matherr(&exc)) {
                  if (FD_LIB_VERSION == FD_SVID_) {
                    (void) WRITE2("fd_atanh: FD_SING error\n", 18);
                  }
                  errno = EDOM;
                }
                break;
	    case 32:
		/* fd_scalb overflow; SVID also returns +-HUGE_VAL */
		exc.type = FD_OVERFLOW;
		exc.name = "fd_scalb";
		exc.retval = x > zero ? HUGE_VAL : -HUGE_VAL;
		if (FD_LIB_VERSION == FD_POSIX_)
		  errno = ERANGE;
		else if (!fd_matherr(&exc)) {
			errno = ERANGE;
		}
		break;
	    case 33:
		/* fd_scalb underflow */
		exc.type = FD_UNDERFLOW;
		exc.name = "fd_scalb";
		exc.retval = fd_copysign(zero,x);
		if (FD_LIB_VERSION == FD_POSIX_)
		  errno = ERANGE;
		else if (!fd_matherr(&exc)) {
			errno = ERANGE;
		}
		break;
	    case 34:
		/* fd_j0(|x|>FD_X_TLOSS) */
                exc.type = FD_TLOSS;
                exc.name = "fd_j0";
                exc.retval = zero;
                if (FD_LIB_VERSION == FD_POSIX_)
                        errno = ERANGE;
                else if (!fd_matherr(&exc)) {
                        if (FD_LIB_VERSION == FD_SVID_) {
                                (void) WRITE2(exc.name, 2);
                                (void) WRITE2(": FD_TLOSS error\n", 14);
                        }
                        errno = ERANGE;
                }
		break;
	    case 35:
		/* fd_y0(x>FD_X_TLOSS) */
                exc.type = FD_TLOSS;
                exc.name = "fd_y0";
                exc.retval = zero;
                if (FD_LIB_VERSION == FD_POSIX_)
                        errno = ERANGE;
                else if (!fd_matherr(&exc)) {
                        if (FD_LIB_VERSION == FD_SVID_) {
                                (void) WRITE2(exc.name, 2);
                                (void) WRITE2(": FD_TLOSS error\n", 14);
                        }
                        errno = ERANGE;
                }
		break;
	    case 36:
		/* fd_j1(|x|>FD_X_TLOSS) */
                exc.type = FD_TLOSS;
                exc.name = "fd_j1";
                exc.retval = zero;
                if (FD_LIB_VERSION == FD_POSIX_)
                        errno = ERANGE;
                else if (!fd_matherr(&exc)) {
                        if (FD_LIB_VERSION == FD_SVID_) {
                                (void) WRITE2(exc.name, 2);
                                (void) WRITE2(": FD_TLOSS error\n", 14);
                        }
                        errno = ERANGE;
                }
		break;
	    case 37:
		/* fd_y1(x>FD_X_TLOSS) */
                exc.type = FD_TLOSS;
                exc.name = "fd_y1";
                exc.retval = zero;
                if (FD_LIB_VERSION == FD_POSIX_)
                        errno = ERANGE;
                else if (!fd_matherr(&exc)) {
                        if (FD_LIB_VERSION == FD_SVID_) {
                                (void) WRITE2(exc.name, 2);
                                (void) WRITE2(": FD_TLOSS error\n", 14);
                        }
                        errno = ERANGE;
                }
		break;
	    case 38:
		/* fd_jn(|x|>FD_X_TLOSS) */
                exc.type = FD_TLOSS;
                exc.name = "fd_jn";
                exc.retval = zero;
                if (FD_LIB_VERSION == FD_POSIX_)
                        errno = ERANGE;
                else if (!fd_matherr(&exc)) {
                        if (FD_LIB_VERSION == FD_SVID_) {
                                (void) WRITE2(exc.name, 2);
                                (void) WRITE2(": FD_TLOSS error\n", 14);
                        }
                        errno = ERANGE;
                }
		break;
	    case 39:
		/* fd_yn(x>FD_X_TLOSS) */
                exc.type = FD_TLOSS;
                exc.name = "fd_yn";
                exc.retval = zero;
                if (FD_LIB_VERSION == FD_POSIX_)
                        errno = ERANGE;
                else if (!fd_matherr(&exc)) {
                        if (FD_LIB_VERSION == FD_SVID_) {
                                (void) WRITE2(exc.name, 2);
                                (void) WRITE2(": FD_TLOSS error\n", 14);
                        }
                        errno = ERANGE;
                }
		break;
	    case 40:
		/* fd_gamma(fd_finite) overflow */
		exc.type = FD_OVERFLOW;
		exc.name = "fd_gamma";
                if (FD_LIB_VERSION == FD_SVID_)
                  exc.retval = FD_HUGE;
                else
                  exc.retval = HUGE_VAL;
                if (FD_LIB_VERSION == FD_POSIX_)
		  errno = ERANGE;
                else if (!fd_matherr(&exc)) {
                  errno = ERANGE;
                }
		break;
	    case 41:
		/* fd_gamma(-integer) or fd_gamma(0) */
		exc.type = FD_SING;
		exc.name = "fd_gamma";
                if (FD_LIB_VERSION == FD_SVID_)
                  exc.retval = FD_HUGE;
                else
                  exc.retval = HUGE_VAL;
		if (FD_LIB_VERSION == FD_POSIX_)
		  errno = EDOM;
		else if (!fd_matherr(&exc)) {
		  if (FD_LIB_VERSION == FD_SVID_) {
			(void) WRITE2("fd_gamma: FD_SING error\n", 18);
		      }
		  errno = EDOM;
		}
		break;
	    case 42:
		/* fd_pow(NaN,0.0) */
		/* error only if FD_LIB_VERSION == FD_SVID_ & _XOPEN_ */
		exc.type = FD_DOMAIN;
		exc.name = "fd_pow";
		exc.retval = x;
		if (FD_LIB_VERSION == FD_IEEE_ ||
		    FD_LIB_VERSION == FD_POSIX_) exc.retval = 1.0;
		else if (!fd_matherr(&exc)) {
			errno = EDOM;
		}
		break;
	}
	return exc.retval;
}
