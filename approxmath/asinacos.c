#include "am_internal.h"

float am_asinf(float xx)
{
	float a, x, z;
	int sign, flag;

	x = xx;

	if(x > 0.f)
	{
		sign = 1;
		a = x;
	}
	else
	{
		sign = -1;
		a = -x;
	}

	if(a > 1.0f)
		return 0.0f;

	if(a < 1.0e-4f)
	{
		z = a;
		goto done;
	}

	if(a > 0.5f)
	{
		z = gMf(0.5f, gSf(1.0f, a));
		x = gSqrtf(z);
		flag = 1;
	}
	else
	{
		x = a;
		z = gMf(x, x);
		flag = 0;
	}

	z =
	gAf(gMf(gMf(gAf(gMf(gAf(gMf(gAf(gMf(gAf(gMf(4.2163199048E-2f, z)
	  , 2.4181311049E-2f), z)
	  , 4.5470025998E-2f), z)
	  , 7.4953002686E-2f), z)
	  , 1.6666752422E-1f), z), x)
	  , x);

	if(flag != 0)
	{
		z = gAf(z, z);
		z = gSf(PIO2F, z);
	}
done:
	if(sign < 0)
		z = -z;
	return z;
}

float am_acosf(float x)
{
	if(x < -1.0f)
		goto domerr;

	if(x < -0.5f)
		return gSf(PIF, gMf(2.0f, am_asinf(gSqrtf(gMf(0.5f, gAf(1.0f,x))))));

	if(x > 1.0f)
	{
domerr:
		return 0.0f; // TODO: NaN?
	}

	if(x > 0.5f)
		return gMf(2.0f, am_asinf(gSqrtf(gMf(0.5f,gSf(1.0f,x)))));

	return gSf(PIO2F, am_asinf(x));
}
