#include "am_internal.h"

float am_expf(float x)
{
	float r;
	_mm_store_ss(&r, am_exp_ss(_mm_load_ss(&x)));
	return r;
}

float am_expf_2(float x)
{
	float r;
	_mm_store_ss(&r, am_exp_ss_2(_mm_load_ss(&x)));
	return r;
}

float am_exp2f(float x)
{
	float r;
	_mm_store_ss(&r, am_exp2_ss(_mm_load_ss(&x)));
	return r;
}


__m128 __declspec(naked) __cdecl am_exp_ss(__m128 x)
{
	__asm
	{
		minss	xmm0, _ps_exp_hi
		movss	xmm1, _ps_exp_rln2
		maxss	xmm0, _ps_exp_lo
		mulss	xmm1, xmm0
		movss	xmm7, _ps_am_0
		addss	xmm1, _ps_am_0p5
		xor		ecx, ecx

		mov		edx, 1
		comiss	xmm1, xmm7
		cvttss2si	eax, xmm1
		cmovc	ecx, edx  // 'c' is 'lt' for comiss
		sub		eax, ecx

		cvtsi2ss	xmm1, eax
		add		eax, 0x7f

		movss	xmm2, xmm1
		mulss	xmm1, _ps_exp_c1
		mulss	xmm2, _ps_exp_c2
		subss	xmm0, xmm1
		shl		eax, 23
		subss	xmm0, xmm2

		movss	xmm2, xmm0
		mov 	[esp - 4], eax
		mulss	xmm2, xmm2

		movss	xmm6, _ps_exp_q0
		movss	xmm4, _ps_exp_p0

		mulss	xmm6, xmm2
		movss	xmm7, _ps_exp_q1
		mulss	xmm4, xmm2
		movss	xmm5, _ps_exp_p1

		addss	xmm6, xmm7
		addss	xmm4, xmm5

		movss	xmm7, _ps_exp_q2
		mulss	xmm6, xmm2
		mulss	xmm4, xmm2

		addss	xmm6, xmm7
		mulss	xmm4, xmm0

		movss	xmm7, _ps_exp_q3
		mulss	xmm6, xmm2
		addss	xmm4, xmm0
		addss	xmm6, xmm7
		movss	xmm0, [esp - 4]

		subss	xmm6, xmm4
		rcpss	xmm6, xmm6
		movss	xmm7, _ps_am_1
		mulss	xmm4, xmm6
		addss	xmm4, xmm4
		addss	xmm4, xmm7

		mulss	xmm0, xmm4

		//ret		16
		ret
	}
}

__m128 __declspec(naked) __cdecl am_exp_ss_2(__m128 x)
{
	__asm
	{
		minss	xmm0, _ps_exp_hi
		movss	xmm1, _ps_exp_rln2
		maxss	xmm0, _ps_exp_lo
		mulss	xmm1, xmm0
		movss	xmm7, _ps_am_0
		addss	xmm1, _ps_am_0p5
		xor		ecx, ecx

		mov		edx, 1
		comiss	xmm1, xmm7
		cvttss2si	eax, xmm1
		cmovc	ecx, edx  // 'c' is 'lt' for comiss
		sub		eax, ecx

		cvtsi2ss	xmm1, eax
		add		eax, 0x7f

		movss	xmm2, xmm1
		mulss	xmm1, _ps_exp_c1
		mulss	xmm2, _ps_exp_c2
		subss	xmm0, xmm1
		shl		eax, 23
		subss	xmm0, xmm2

		movss	xmm2, xmm0
		mov 	[esp - 4], eax
		mulss	xmm2, xmm2

		movss	xmm6, _ps_exp_q0
		movss	xmm4, _ps_exp_p0

		mulss	xmm6, xmm2
		movss	xmm7, _ps_exp_q1
		mulss	xmm4, xmm2
		movss	xmm5, _ps_exp_p1

		addss	xmm6, xmm7
		addss	xmm4, xmm5

		movss	xmm7, _ps_exp_q2
		mulss	xmm6, xmm2
		mulss	xmm4, xmm2

		addss	xmm6, xmm7
		mulss	xmm4, xmm0

		movss	xmm7, _ps_exp_q3
		mulss	xmm6, xmm2
		addss	xmm4, xmm0
		addss	xmm6, xmm7
		movss	xmm0, [esp - 4]

		subss	xmm6, xmm4
		movss	xmm7, _ps_am_1
		divss   xmm7, xmm6
		movss	xmm6, _ps_am_1
		mulss	xmm4, xmm7
		addss	xmm4, xmm4
		addss	xmm4, xmm6

		mulss	xmm0, xmm4

		ret
	}
}

__m128 __declspec(naked) __cdecl am_exp2_ss(__m128 x)
{
	__asm
	{
		minss	xmm0, _ps_exp2_hi
		movss	xmm5, _ps_am_0p5
		maxss	xmm0, _ps_exp2_lo
		xorps	xmm7, xmm7
		addss	xmm5, xmm0
		xor		ecx, ecx

		mov		edx, 1
		comiss	xmm5, xmm7
		cvttss2si	eax, xmm5
		cmovc	ecx, edx  // 'c' is 'lt' for comiss
		sub		eax, ecx

		cvtsi2ss	xmm5, eax
		add		eax, 0x7f

		subss	xmm0, xmm5

		movss	xmm2, xmm0
		mulss	xmm2, xmm2

		movss	xmm6, _ps_exp2_q0
		movss	xmm4, _ps_exp2_p0

		mulss	xmm6, xmm2
		movss	xmm7, _ps_exp2_q1
		mulss	xmm4, xmm2
		movss	xmm5, _ps_exp2_p1

		shl		eax, 23
		addss	xmm6, xmm7
		addss	xmm4, xmm5

		movss	xmm5, _ps_exp2_p2
		mulss	xmm4, xmm2

		addss	xmm4, xmm5

		mulss	xmm4, xmm0

		mov 	[esp - 4], eax
		subss	xmm6, xmm4
		movss	xmm7, _ps_am_1
		rcpss	xmm6, xmm6
		mulss	xmm4, xmm6
		movss	xmm0, [esp - 4]
		addss	xmm4, xmm4
		addss	xmm4, xmm7

		mulss	xmm0, xmm4

		ret
	}
}