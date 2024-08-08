#include "am_internal.h"

am_pair am_sincosf(float x)
{
	am_pair r;
	float temp1, temp2;

	__asm
	{
		movss	xmm1, _ps_am_inv_sign_mask
		mov		eax, x
		mulss	xmm0, _ps_am_2_o_pi
		andps	xmm0, xmm1
		and		eax, 0x80000000

		cvttss2si	edx, xmm0
		mov		ecx, edx
		mov		esi, edx
		add		edx, 0x1
		shl		ecx, (31 - 1)
		shl		edx, (31 - 1)

		movss	xmm4, _ps_am_1
		cvtsi2ss	xmm3, esi
		mov		temp1, eax
		and		esi, 0x1

		subss	xmm0, xmm3
		movss	xmm3, _sincos_inv_masks[esi * 4]
		minss	xmm0, xmm4

		subss	xmm4, xmm0

		movss	xmm6, xmm4
		andps	xmm4, xmm3
		and		ecx, 0x80000000
		movss	xmm2, xmm3
		andnps	xmm3, xmm0
		and		edx, 0x80000000
		movss	xmm7, temp1
		andps	xmm0, xmm2
		mov		temp1, ecx
		mov		temp2, edx
		orps	xmm4, xmm3

		andnps	xmm2, xmm6
		orps	xmm0, xmm2

		movss	xmm2, temp1
		movss	xmm1, xmm0
		movss	xmm5, xmm4
		xorps	xmm7, xmm2
		movss	xmm3, _ps_sincos_p3
		mulss	xmm0, xmm0
		mulss	xmm4, xmm4
		movss	xmm2, xmm0
		movss	xmm6, xmm4
		orps	xmm1, xmm7
		movss	xmm7, _ps_sincos_p2
		mulss	xmm0, xmm3
		mulss	xmm4, xmm3
		movss	xmm3, _ps_sincos_p1
		addss	xmm0, xmm7
		addss	xmm4, xmm7
		movss	xmm7, _ps_sincos_p0
		mulss	xmm0, xmm2
		mulss	xmm4, xmm6
		addss	xmm0, xmm3
		addss	xmm4, xmm3
		movss	xmm3, temp2
		mulss	xmm0, xmm2
		mulss	xmm4, xmm6
		orps	xmm5, xmm3
		addss	xmm0, xmm7
		addss	xmm4, xmm7
		mulss	xmm0, xmm1
		mulss	xmm4, xmm5

		movss r.first, xmm0
		movss r.second, xmm4

		//ret		16 + 4 + 4 + 8
		//ret
	}
	return r;
}

float am_sinf_inline(float x)
{
	__asm
	{
		movss   xmm0, x
		movss	xmm1, _ps_am_inv_sign_mask
		mov		eax, x
		mulss	xmm0, _ps_am_2_o_pi
		andps	xmm0, xmm1
		and		eax, 0x80000000

		cvttss2si	ecx, xmm0
		movss	xmm1, _ps_am_1
		mov		edx, ecx
		shl		edx, (31 - 1)
		cvtsi2ss	xmm2, ecx
		and		ecx, 0x1
		and		edx, 0x80000000

		subss	xmm0, xmm2
		movss	xmm6, _sincos_masks[ecx * 4]
		minss	xmm0, xmm1

		movss	xmm5, _ps_sincos_p3
		subss	xmm1, xmm0

		andps	xmm1, xmm6
		andnps	xmm6, xmm0
		orps	xmm1, xmm6
		movss	xmm4, _ps_sincos_p2
		movss	xmm0, xmm1

		mulss	xmm1, xmm1
		movss	xmm7, _ps_sincos_p1
		xor		eax, edx
		movss	xmm2, xmm1
		mulss	xmm1, xmm5
		movss	xmm5, _ps_sincos_p0
		mov		x, eax
		addss	xmm1, xmm4
		mulss	xmm1, xmm2
		movss	xmm3, x
		addss	xmm1, xmm7
		mulss	xmm1, xmm2
		orps	xmm0, xmm3
		addss	xmm1, xmm5
		mulss	xmm0, xmm1
		movss   x, xmm0
	}

	return x;
}

float am_sinf(float x)
{
	unsigned a, c, d;
	__m128 xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;
	xmm0 = _mm_load_ss(&x);
	xmm1 = _mm_load_ss((float*)_ps_am_inv_sign_mask);
	a = *(unsigned*)&x;
	xmm0 = _mm_mul_ss(xmm0, _mm_load_ss(_ps_am_2_o_pi));
	xmm0 = _mm_and_ps(xmm0, xmm1);
	a &= 0x80000000;

	c = _mm_cvttss_si32(xmm0);
	xmm1 = _mm_load_ss(_ps_am_1);
	d = c;
	d <<= (31 - 1);
	xmm2 = _mm_cvtsi32_ss(xmm2, c);
	c &= 1;
	d &= 0x80000000;

	xmm0 = _mm_sub_ss(xmm0, xmm2);
	xmm6 = _mm_load_ss((float*)&_sincos_masks[c]);
	xmm0 = _mm_min_ss(xmm0, xmm1);

	xmm5 = _mm_load_ss(_ps_sincos_p3);
	xmm1 = _mm_sub_ss(xmm1, xmm0);

	xmm1 = _mm_and_ps(xmm1, xmm6);
	xmm6 = _mm_andnot_ps(xmm6, xmm0);
	xmm1 = _mm_or_ps(xmm1, xmm6);
	xmm4 = _mm_load_ss(_ps_sincos_p2);
	xmm0 = xmm1;

	xmm1 = _mm_mul_ss(xmm1, xmm1);
	xmm7 = _mm_load_ss(_ps_sincos_p1);
	a ^= d;
	xmm2 = xmm1;
	xmm1 = _mm_mul_ss(xmm1, xmm5);
	xmm5 = _mm_load_ss(_ps_sincos_p0);
	xmm1 = _mm_add_ss(xmm1, xmm4);
	xmm1 = _mm_mul_ss(xmm1, xmm2);
	xmm3 = _mm_load_ss((float*)&a);
	xmm1 = _mm_add_ss(xmm1, xmm7);
	xmm1 = _mm_mul_ss(xmm1, xmm2);
	xmm0 = _mm_or_ps(xmm0, xmm3);
	xmm1 = _mm_add_ss(xmm1, xmm5);
	xmm0 = _mm_mul_ss(xmm0, xmm1);

	_mm_store_ss(&x, xmm0);

	return x;
}

/*
float am_sinf(float x)
{
	float r;
	_mm_store_ss(&r, am_sin_ss(_mm_load_ss(&x)));
	return r;
}*/

float am_sinf_2(float x)
{
	float r;
	_mm_store_ss(&r, am_sin_ess(_mm_load_ss(&x)));
	return r;
}

float am_cosf(float x)
{
	float r;
	_mm_store_ss(&r, am_cos_ss(_mm_load_ss(&x)));
	return r;
}

__m128 __declspec(naked) __cdecl am_sin_ss(__m128 x)
{
	__asm
	{
		movss	[esp - 4], xmm0
		movss	xmm1, _ps_am_inv_sign_mask
		mov		eax, [esp - 4]
		mulss	xmm0, _ps_am_2_o_pi
		andps	xmm0, xmm1
		and		eax, 0x80000000

		cvttss2si	ecx, xmm0
		movss	xmm1, _ps_am_1
		mov		edx, ecx
		shl		edx, (31 - 1)
		cvtsi2ss	xmm2, ecx
		and		ecx, 0x1
		and		edx, 0x80000000

		subss	xmm0, xmm2
		movss	xmm6, _sincos_masks[ecx * 4]
		minss	xmm0, xmm1

		movss	xmm5, _ps_sincos_p3
		subss	xmm1, xmm0

		andps	xmm1, xmm6
		andnps	xmm6, xmm0
		orps	xmm1, xmm6
		movss	xmm4, _ps_sincos_p2
		movss	xmm0, xmm1

		mulss	xmm1, xmm1
		movss	xmm7, _ps_sincos_p1
		xor		eax, edx
		movss	xmm2, xmm1
		mulss	xmm1, xmm5
		movss	xmm5, _ps_sincos_p0
		mov		[esp - 4], eax
		addss	xmm1, xmm4
		mulss	xmm1, xmm2
		movss	xmm3, [esp - 4]
		addss	xmm1, xmm7
		mulss	xmm1, xmm2
		orps	xmm0, xmm3
		addss	xmm1, xmm5
		mulss	xmm0, xmm1

		//ret		16
		ret
	}
}

__m128 __declspec(naked) __cdecl am_cos_ss(__m128 x)
{
	__asm
	{
		movss	xmm1, _ps_am_inv_sign_mask
		andps	xmm0, xmm1
		addss	xmm0, _ps_am_pi_o_2
		mulss	xmm0, _ps_am_2_o_pi

		cvttss2si	ecx, xmm0
		movss	xmm5, _ps_am_1
		mov		edx, ecx
		shl		edx, (31 - 1)
		cvtsi2ss	xmm1, ecx
		and		edx, 0x80000000
		and		ecx, 0x1

		subss	xmm0, xmm1
		movss	xmm6, _sincos_masks[ecx * 4]
		minss	xmm0, xmm5

		movss	xmm1, _ps_sincos_p3
		subss	xmm5, xmm0

		andps	xmm5, xmm6
		movss	xmm7, _ps_sincos_p2
		andnps	xmm6, xmm0
		mov		[esp - 4], edx
		orps	xmm5, xmm6
		movss	xmm0, xmm5

		mulss	xmm5, xmm5
		movss	xmm4, _ps_sincos_p1
		movss	xmm2, xmm5
		mulss	xmm5, xmm1
		movss	xmm1, _ps_sincos_p0
		addss	xmm5, xmm7
		mulss	xmm5, xmm2
		movss	xmm3, [esp - 4]
		addss	xmm5, xmm4
		mulss	xmm5, xmm2
		orps	xmm0, xmm3
		addss	xmm5, xmm1
		mulss	xmm0, xmm5

		ret
	}
}


__m128 __declspec(naked) __cdecl am_sin_ess(__m128 x)
{
	__asm
	{
		movaps	xmm7, xmm0
		movss	xmm1, _ps_am_inv_sign_mask
		movss	xmm2, _ps_am_sign_mask
		movss	xmm3, _ps_am_2_o_pi
		andps	xmm0, xmm1
		andps	xmm7, xmm2
		mulss	xmm0, xmm3

		pxor	xmm3, xmm3
		movd	xmm5, _epi32_1
		movss	xmm4, _ps_am_1
		cvttps2dq	xmm2, xmm0
		pand	xmm5, xmm2
		movd	xmm1, _epi32_2
		pcmpeqd	xmm5, xmm3
		cvtdq2ps	xmm6, xmm2
		pand	xmm2, xmm1
		pslld	xmm2, (31 - 1)

		subss	xmm0, xmm6
		movss	xmm3, _ps_sincos_p3
		minss	xmm0, xmm4
		subss	xmm4, xmm0
		andps	xmm0, xmm5
		andnps	xmm5, xmm4
		orps	xmm0, xmm5

		movaps	xmm1, xmm0
		movss	xmm4, _ps_sincos_p2
		mulss	xmm0, xmm0
		xorps	xmm2, xmm7
		movss	xmm5, _ps_sincos_p1
		orps	xmm1, xmm2
		movaps	xmm7, xmm0
		mulss	xmm0, xmm3
		movss	xmm6, _ps_sincos_p0
		addss	xmm0, xmm4
		mulss	xmm0, xmm7
		addss	xmm0, xmm5
		mulss	xmm0, xmm7
		addss	xmm0, xmm6
		mulss	xmm0, xmm1

		ret
	}
}