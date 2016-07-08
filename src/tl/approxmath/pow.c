#include "am_internal.h"
#include <math.h>

float am_powf(float x, float y)
{
	float r;
	//_mm_store_ss(&r, am_pow_ss(_mm_load_ss(&x), _mm_load_ss(&y)));

	float temp;

	__asm
	{
		movss   xmm0, x
		movss   xmm1, y

		xorps	xmm7, xmm7
		comiss	xmm7, xmm0
		movss	xmm7, _ps_am_inv_mant_mask
		maxss	xmm0, _ps_am_min_norm_pos  // cut off denormalized stuff
		jnc		l_zerobase
		movss	xmm3, _ps_am_1
		movss	temp, xmm0

		andps	xmm0, xmm7
		orps	xmm0, xmm3
		movss	xmm7, xmm0

		addss	xmm7, xmm3
		subss	xmm0, xmm3
		mov		edx, temp
		rcpss	xmm7, xmm7
		mulss	xmm0, xmm7
		addss	xmm0, xmm0

		shr		edx, 23

		movss	xmm4, _ps_log_p0
		movss	xmm6, _ps_log_q0

		sub		edx, 0x7f
		movss	xmm2, xmm0
		mulss	xmm2, xmm2

		mulss	xmm4, xmm2
		movss	xmm5, _ps_log_p1
		mulss	xmm6, xmm2
		cvtsi2ss	xmm3, edx
		movss	xmm7, _ps_log_q1

		addss	xmm4, xmm5
		mulss	xmm3, xmm1
		addss	xmm6, xmm7

		movss	xmm5, _ps_log_p2
		mulss	xmm4, xmm2
		movss	xmm7, _ps_log_q2
		mulss	xmm6, xmm2

		addss	xmm4, xmm5
		mulss	xmm1, _ps_log2_c0
		addss	xmm6, xmm7

		mulss	xmm4, xmm2
		rcpss	xmm6, xmm6

		mulss	xmm6, xmm0
		mulss	xmm4, xmm6
		movss	xmm6, _ps_exp2_hi
		addss	xmm0, xmm4
		movss	xmm4, _ps_exp2_lo
		xorps	xmm7, xmm7
		movss	xmm5, _ps_am_0p5
		mulss	xmm0, xmm1

		addss	xmm0, xmm3
		xor		ecx, ecx

		minss	xmm0, xmm6
		mov		edx, 1
		maxss	xmm0, xmm4

		addss	xmm5, xmm0

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

		mov 	temp, eax
		subss	xmm6, xmm4
		movss	xmm7, _ps_am_1
		rcpss	xmm6, xmm6
		mulss	xmm4, xmm6
		movss	xmm0, temp
		addss	xmm4, xmm4
		addss	xmm4, xmm7

		mulss	xmm0, xmm4
		movss   r, xmm0

		//ret
		jmp l_exit

l_zerobase:
		xorps	xmm0, xmm0
		movss   r, xmm0
l_exit:
	}
	return r;
}

/*
__m128 __declspec(naked) __cdecl am_pow_ss(__m128 x, __m128 y)
{
	__asm
	{
		xorps	xmm7, xmm7
		comiss	xmm7, xmm0
		movss	xmm7, _ps_am_inv_mant_mask
		maxss	xmm0, _ps_am_min_norm_pos  // cut off denormalized stuff
		jnc		l_zerobase
		movss	xmm3, _ps_am_1
		movss	[esp - 4], xmm0

		andps	xmm0, xmm7
		orps	xmm0, xmm3
		movss	xmm7, xmm0

		addss	xmm7, xmm3
		subss	xmm0, xmm3
		mov		edx, [esp - 4]
		rcpss	xmm7, xmm7
		mulss	xmm0, xmm7
		addss	xmm0, xmm0

		shr		edx, 23

		movss	xmm4, _ps_log_p0
		movss	xmm6, _ps_log_q0

		sub		edx, 0x7f
		movss	xmm2, xmm0
		mulss	xmm2, xmm2

		mulss	xmm4, xmm2
		movss	xmm5, _ps_log_p1
		mulss	xmm6, xmm2
		cvtsi2ss	xmm3, edx
		movss	xmm7, _ps_log_q1

		addss	xmm4, xmm5
		mulss	xmm3, xmm1
		addss	xmm6, xmm7

		movss	xmm5, _ps_log_p2
		mulss	xmm4, xmm2
		movss	xmm7, _ps_log_q2
		mulss	xmm6, xmm2

		addss	xmm4, xmm5
		mulss	xmm1, _ps_log2_c0
		addss	xmm6, xmm7

		mulss	xmm4, xmm2
		rcpss	xmm6, xmm6

		mulss	xmm6, xmm0
		mulss	xmm4, xmm6
		movss	xmm6, _ps_exp2_hi
		addss	xmm0, xmm4
		movss	xmm4, _ps_exp2_lo
		xorps	xmm7, xmm7
		movss	xmm5, _ps_am_0p5
		mulss	xmm0, xmm1

		addss	xmm0, xmm3
		xor		ecx, ecx

		minss	xmm0, xmm6
		mov		edx, 1
		maxss	xmm0, xmm4

		addss	xmm5, xmm0

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

l_zerobase:
		xorps	xmm0, xmm0

		ret
	}
}*/

static float cephes_C1 =   0.693359375f;
static float cephes_C2 =  -2.12194440e-4f;

float cephes_expf(float xx)
{
#if 0
	float x, z;
	int n;

	x = xx;

	if(x > MAXLOGF)
	{
		return MAXNUMF;
	}

	if(x < MINLOGF)
	{
		return 0.f;
	}

	/* Express e**x = e**g 2**n
	 *   = e**g e**( n loge(2) )
	 *   = e**( g + n loge(2) )
	 */
	z = floorf(gAf(gMf(LOG2EF, x), 0.5f)); /* floor() truncates toward -infinity. */
	x = gSf(x, gMf(z, cephes_C1));
	x = gSf(x, gMf(z, cephes_C2));
	n = (int)z;

	z = gMf(x, x);
	/* Theoretical peak relative error in [-0.5, +0.5] is 4.2e-9. */
	z =
	gAf(gAf(gMf(gAf(gMf(gAf(gMf(gAf(gMf(gAf(gMf(gAf(gMf(1.9875691500E-4f, x)
	   , 1.3981999507E-3f), x)
	   , 8.3334519073E-3f), x)
	   , 4.1665795894E-2f), x)
	   , 1.6666665459E-1f), x)
	   , 5.0000001201E-1f), z), x), 1.0f);

	/* multiply by power of 2 */
	x = ldexpf(z, n);
	return x;
#else

	float x, z;
	int n;

	x = xx;


	if( x > MAXLOGF)
		{
		return( MAXNUMF );
		}

	if( x < MINLOGF )
		{
		return(0.0);
		}

	/* Express e**x = e**g 2**n
	 *   = e**g e**( n loge(2) )
	 *   = e**( g + n loge(2) )
	 */
	z = floorf(gAf(gMf(LOG2EF, x), 0.5)); /* floor() truncates toward -infinity. */
	x = gSf(x, gMf(z, cephes_C1));
	x = gSf(x, gMf(z, cephes_C2));
	n = (int)z;

	z = gMf(x, x);
	/* Theoretical peak relative error in [-0.5, +0.5] is 4.2e-9. */
	z =
	(gMf(gAf(gMf(gAf(gMf(gAf(gMf(gAf(gMf(1.9875691500E-4f, x)
	   , 1.3981999507E-3f), x)
	   , 8.3334519073E-3f), x)
	   , 4.1665795894E-2f), x)
	   , 1.6666665459E-1f), x)
	   + 5.0000001201E-1f) * z
	   + x
	   + 1.0f;

	/* multiply by power of 2 */
	x = ldexpf( z, n );

	return( x );
#endif
}
