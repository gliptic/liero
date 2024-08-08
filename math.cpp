#include "math.hpp"
#include <gvl/cstdint.hpp>
#include <cmath>

fixedvec cossinTable[128];

uint32_t sqr(uint32_t op)
{
    uint32_t res = 0;
    uint32_t one = 1uL << 30; // The second-to-top bit is set: use 1u << 14 for uint16_t type; use 1uL<<30 for uint32_t type

    // "one" starts at the highest power of four <= than the argument.
    while (one > op)
        one >>= 2;

    while (one != 0)
    {
        if (op >= res + one)
        {
            op -= res + one;
            res += 2 * one;
        }
        res >>= 1;
        one >>= 2;
    }
    return res;
}

int vectorLength(int x, int y)
{
	return int(sqr(x*x + y*y));
}

struct FP
{
	FP(int64_t s, int bits)
	: s(s), bits(bits)
	{
	}

	void reduce(int tobits)
	{
		int64_t lim = (1ll << tobits);

		while (s < (-lim - 1) || s > lim)
		{
			s >>= 1;
			--bits;
		}
	}

	int64_t reducedfrac(int tobits)
	{
		int64_t rs = s;
		int rbits = bits;
		while (rbits > 60)
		{
			rs >>= 1;
			--rbits;
		}

		return rs << (tobits - rbits);
	}

	int64_t s;
	int bits;
};

void precomputeTables()
{
	int scalebits = 28;
	int32_t scale = 13176795; // (2pi / 128) << scalebits

	for(int i = 0; i < 128; ++i)
	{
		int64_t rf = 0;
		int32_t c = -1;
		int32_t xf = i * scale;

		// Simple Taylor series. Performance is not important.
		FP num(xf, scalebits);
		for(int t = 1; t < 26; )
		{
			rf += c * num.reducedfrac(60);

			num.s /= ++t;
			num.reduce(31);
			num.s = num.s * xf;
			num.bits += scalebits;

			num.s /= ++t;
			num.reduce(31);
			num.s = num.s * xf;
			num.bits += scalebits;

			c = -c;
		}

		int shift = 60 - 16;

		rf += (1LL << (shift - 1)); // Correct rounding

		int32_t r = (int32_t)(rf >> shift);

		cossinTable[i].x = r;
		cossinTable[(i + 32) & 0x7f].y = r;
	}
}
