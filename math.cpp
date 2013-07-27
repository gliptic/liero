#include "math.hpp"
#include "reader.hpp"
#include <cmath>

fixed sinTable[128];
fixed cosTable[128];

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

void loadTablesFromEXE()
{
	for(int i = 0; i < 128; ++i)
	{
		fixed c, s;
		double a = i * 0.04908738521234051935097880286374 + 1.5707963267948966192313216916398;
		double cf = std::cos(a) * 65536;
		double sf = std::sin(a) * 65536;
		c = int(cf > 0.0 ? std::floor(cf + 0.5) : std::ceil(cf - 0.5));
		s = int(sf > 0.0 ? std::floor(sf + 0.5) : std::ceil(sf - 0.5));

		cosTable[i] = c;
		sinTable[i] = s;
	}
}
