#include "am_internal.h"

float am_tanf(float x)
{
	float r;
	_mm_store_ss(&r, am_tan_ess(_mm_load_ss(&x)));
	return r;
}

float am_tanf_2(float x)
{
	float r;
	_mm_store_ss(&r, am_tan_ess_2(_mm_load_ss(&x)));
	return r;
}

__m128 __declspec(naked) __cdecl am_tan_ess(__m128 x)  // any x
{
	__asm
	{
		movss	xmm1, _ps_am_inv_sign_mask
		movd	eax, xmm0
		andps	xmm0, xmm1
		movaps	xmm1, xmm0
		mulss	xmm0, _ps_am_4_o_pi

		cvttss2si	edx, xmm0
		and		eax, 0x80000000

		mov		ecx, 0x1
		movd	xmm7, eax
		mov		eax, 0x7

		movss	xmm5, _ps_am_1

		and		ecx, edx
		and		eax, edx
		add		edx, ecx
		add		eax, ecx

		cvtsi2ss	xmm0, edx
		xorps	xmm6, xmm6

		mulss	xmm0, _ps_am_pi_o_4
		subss	xmm1, xmm0
		movss	xmm2, _ps_tan_p2
		minss	xmm1, xmm5
		movss	xmm3, _ps_tan_q3
		movaps	xmm0, xmm1
		mulss	xmm1, xmm1

		mulss	xmm2, xmm1
		addss	xmm3, xmm1
		addss	xmm2, _ps_tan_p1
		mulss	xmm3, xmm1
		mulss	xmm2, xmm1
		addss	xmm3, _ps_tan_q2
		addss	xmm2, _ps_tan_p0
		mulss	xmm3, xmm1
		mulss	xmm2, xmm1
		addss	xmm3, _ps_tan_q1
		xorps	xmm0, xmm7
		mulss	xmm3, xmm1
		mulss	xmm2, xmm0
		addss	xmm3, _ps_tan_q0

		rcpss	xmm4, xmm3
		mulss	xmm3, xmm4
		mulss	xmm3, xmm4
		addss	xmm4, xmm4
		test	eax, 0x2
		subss	xmm4, xmm3

		mulss	xmm2, xmm4
		jz		l_cont
		addss	xmm2, xmm0
		comiss	xmm6, xmm1

		rcpss	xmm4, xmm2
		movss	xmm0, _ps_am_sign_mask
		jz		l_pole
		mulss	xmm2, xmm4
		mulss	xmm2, xmm4
		addss	xmm4, xmm4
		subss	xmm4, xmm2
		xorps	xmm0, xmm4

		ret

l_pole:
		movss	xmm1, _ps_tan_poleval
		movaps	xmm3, xmm0
		andps	xmm0, xmm2
		orps	xmm0, xmm1

		xorps	xmm0, xmm3

		ret

l_cont:
		addss	xmm0, xmm2
		ret
	}
}

__m128 __declspec(naked) __cdecl am_tan_ess_2(__m128 x)  // any x
{
	__asm
	{
		movss	xmm1, _ps_am_inv_sign_mask
		movd	eax, xmm0
		andps	xmm0, xmm1
		movaps	xmm1, xmm0
		mulss	xmm0, _ps_am_4_o_pi

		cvttss2si	edx, xmm0
		and		eax, 0x80000000

		mov		ecx, 0x1
		movd	xmm7, eax
		mov		eax, 0x7

		movss	xmm5, _ps_am_1

		and		ecx, edx
		and		eax, edx
		add		edx, ecx
		add		eax, ecx

		cvtsi2ss	xmm0, edx
		xorps	xmm6, xmm6

		mulss	xmm0, _ps_am_pi_o_4
		subss	xmm1, xmm0
		movss	xmm2, _ps_tan_p2
		minss	xmm1, xmm5
		movss	xmm3, _ps_tan_q3
		movaps	xmm0, xmm1
		mulss	xmm1, xmm1

		mulss	xmm2, xmm1
		addss	xmm3, xmm1
		addss	xmm2, _ps_tan_p1
		mulss	xmm3, xmm1
		mulss	xmm2, xmm1
		addss	xmm3, _ps_tan_q2
		addss	xmm2, _ps_tan_p0
		mulss	xmm3, xmm1
		mulss	xmm2, xmm1
		addss	xmm3, _ps_tan_q1
		xorps	xmm0, xmm7
		mulss	xmm3, xmm1
		mulss	xmm2, xmm0
		addss	xmm3, _ps_tan_q0

		rcpss	xmm4, xmm3
		mulss	xmm3, xmm4
		mulss	xmm3, xmm4
		addss	xmm4, xmm4
		test	eax, 0x2
		subss	xmm4, xmm3

		mulss	xmm2, xmm4
		jz		l_cont
		addss	xmm2, xmm0
		comiss	xmm6, xmm1

		//rcpss	xmm4, xmm2
		movss   xmm4, _ps_am_1
		divss   xmm4, xmm2
		movss	xmm0, _ps_am_sign_mask
		jz		l_pole
		mulss	xmm2, xmm4
		mulss	xmm2, xmm4
		addss	xmm4, xmm4
		subss	xmm4, xmm2
		xorps	xmm0, xmm4

		ret

l_pole:
		movss	xmm1, _ps_tan_poleval
		movaps	xmm3, xmm0
		andps	xmm0, xmm2
		orps	xmm0, xmm1

		xorps	xmm0, xmm3

		ret

l_cont:
		addss	xmm0, xmm2
		ret
	}
}