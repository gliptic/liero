#ifndef UUID_92AEC2D0C12340302DEB4DAE68EDBF8A
#define UUID_92AEC2D0C12340302DEB4DAE68EDBF8A

#include "am.h"
#include "../ieee.h"
#include <emmintrin.h>

void __cdecl am_sincos_ss(__m128 x, __m128* s, __m128* c);
__m128 __cdecl am_sin_ss(__m128 x);
__m128 __cdecl am_cos_ss(__m128 x);
__m128 __cdecl am_sin_ess(__m128 x);

__m128 __cdecl am_tan_ess(__m128 x);
__m128 __cdecl am_tan_ess_2(__m128 x);

// NOTE: These depend upon exact behavior of RCPSS
__m128 __cdecl am_exp_ss(__m128 x);
__m128 __cdecl am_exp2_ss(__m128 x);
__m128 __cdecl am_pow_ss(__m128 x, __m128 y);

__m128 __cdecl am_exp_ss_2(__m128 x);

#define AM_PI			(3.14159265358979323846)
//#define AM_PI2          (6.28318530717958647693)
#define AM_PI_O_2       (1.57079632679489661923)
#define AM_PI_O_4       (0.785398163397448309616)
#define AM_2_O_PI       (0.636619772367581343076)
#define AM_4_O_PI       (1.27323954473516268615)
//#define AM_PI_SQR       (9.86960440108935861883)

#define _PS_CONST(Name, Val) \
static const _MM_ALIGN16 float _ps_##Name[4] = { Val, Val, Val, Val }

#define _PS_EXTERN_CONST(Name, Val) \
static const _MM_ALIGN16 float _ps_##Name[4] = { Val, Val, Val, Val }

#define _PS_EXTERN_CONST_TYPE(Name, Type, Val) \
static const _MM_ALIGN16 Type _ps_##Name[4] = { Val, Val, Val, Val }; \

#define _PS_CONST_TYPE(Name, Type, Val) \
static const _MM_ALIGN16 Type _ps_##Name[4] = { Val, Val, Val, Val }; \

#define _EPI32_CONST(Name, Val) \
static const _MM_ALIGN16 __int32 _epi32_##Name[4] = { Val, Val, Val, Val }


_PS_EXTERN_CONST(am_0, 0.0f);
_PS_EXTERN_CONST(am_1, 1.0f);
_PS_EXTERN_CONST(am_m1, -1.0f);
_PS_EXTERN_CONST(am_0p5, 0.5f);
_PS_EXTERN_CONST(am_1p5, 1.5f);
_PS_EXTERN_CONST(am_pi, (float)AM_PI);
_PS_EXTERN_CONST(am_pi_o_2, (float)AM_PI_O_2);
_PS_EXTERN_CONST(am_2_o_pi, (float)AM_2_O_PI);
_PS_EXTERN_CONST(am_pi_o_4, (float)AM_PI_O_4);
_PS_EXTERN_CONST(am_4_o_pi, (float)AM_4_O_PI);
_PS_EXTERN_CONST_TYPE(am_sign_mask, __int32, 0x80000000);
_PS_EXTERN_CONST_TYPE(am_inv_sign_mask, __int32, ~0x80000000);
_PS_EXTERN_CONST_TYPE(am_min_norm_pos, __int32, 0x00800000);
_PS_EXTERN_CONST_TYPE(am_mant_mask, __int32, 0x7f800000);
_PS_EXTERN_CONST_TYPE(am_inv_mant_mask, __int32, ~0x7f800000);

_EPI32_CONST(1, 1);
_EPI32_CONST(2, 2);
_EPI32_CONST(7, 7);
_EPI32_CONST(0x7f, 0x7f);
_EPI32_CONST(0xff, 0xff);

_PS_CONST(exp_hi,	88.3762626647949f);
_PS_CONST(exp_lo,	-88.3762626647949f);

_PS_CONST(exp_rln2, 1.4426950408889634073599f);

_PS_CONST(exp_p0, 1.26177193074810590878e-4f);
_PS_CONST(exp_p1, 3.02994407707441961300e-2f);

_PS_CONST(exp_q0, 3.00198505138664455042e-6f);
_PS_CONST(exp_q1, 2.52448340349684104192e-3f);
_PS_CONST(exp_q2, 2.27265548208155028766e-1f);
_PS_CONST(exp_q3, 2.00000000000000000009e0f);

_PS_CONST(exp_c1, 6.93145751953125e-1f);
_PS_CONST(exp_c2, 1.42860682030941723212e-6f);


_PS_CONST(exp2_hi, 127.4999961853f);
_PS_CONST(exp2_lo, -127.4999961853f);

_PS_CONST(exp2_p0, 2.30933477057345225087e-2f);
_PS_CONST(exp2_p1, 2.02020656693165307700e1f);
_PS_CONST(exp2_p2, 1.51390680115615096133e3f);

_PS_CONST(exp2_q0, 2.33184211722314911771e2f);
_PS_CONST(exp2_q1, 4.36821166879210612817e3f);

_PS_CONST(log_p0, -7.89580278884799154124e-1f);
_PS_CONST(log_p1, 1.63866645699558079767e1f);
_PS_CONST(log_p2, -6.41409952958715622951e1f);

_PS_CONST(log_q0, -3.56722798256324312549e1f);
_PS_CONST(log_q1, 3.12093766372244180303e2f);
_PS_CONST(log_q2, -7.69691943550460008604e2f);

_PS_CONST(log_c0, 0.693147180559945f);
_PS_CONST(log2_c0, 1.44269504088896340735992f);

_PS_CONST(sincos_p0, 0.15707963267948963959e1f);
_PS_CONST(sincos_p1, -0.64596409750621907082e0f);
_PS_CONST(sincos_p2, 0.7969262624561800806e-1f);
_PS_CONST(sincos_p3, -0.468175413106023168e-2f);

static const unsigned __int32 _sincos_masks[] = { 0x0ul, ~0x0ul };
static const unsigned __int32 _sincos_inv_masks[] = { ~0x0ul, 0x0ul };

_PS_CONST(tan_p0, -1.79565251976484877988e7f);
_PS_CONST(tan_p1, 1.15351664838587416140e6f);
_PS_CONST(tan_p2, -1.30936939181383777646e4f);

_PS_CONST(tan_q0, -5.38695755929454629881e7f);
_PS_CONST(tan_q1, 2.50083801823357915839e7f);
_PS_CONST(tan_q2, -1.32089234440210967447e6f);
_PS_CONST(tan_q3, 1.36812963470692954678e4f);

_PS_CONST(tan_poleval, 3.68935e19f);

static float MAXNUMF = 3.4028234663852885981170418348451692544e38f;
static float MAXLOGF = 88.72283905206835f;
static float MINLOGF = -103.278929903431851103f; /* log(2^-149) */
static float LOG2EF = 1.44269504088896341f;
static float LOGE2F = 0.693147180559945309f;
static float SQRTHF = 0.707106781186547524f;
static float PIF = 3.141592653589793238f;
static float PIO2F = 1.5707963267948966192f;
static float PIO4F = 0.7853981633974483096f;
static float MACHEPF = 5.9604644775390625E-8f;

#endif // UUID_92AEC2D0C12340302DEB4DAE68EDBF8A
