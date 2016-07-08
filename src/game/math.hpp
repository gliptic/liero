#ifndef LIERO_MATH_HPP
#define LIERO_MATH_HPP

#include <gvl/math/vec.hpp>

using fixedvec = gvl::ivec2;

typedef int fixed;

inline fixed itof(int v)
{
	return v << 16;
}

inline int ftoi(fixed v)
{
	return v >> 16;
}

inline fixedvec itof(gvl::ivec2 v)
{
	return fixedvec(itof(v.x), itof(v.y));
}

inline gvl::ivec2 ftoi(fixedvec v)
{
	return gvl::ivec2(ftoi(v.x), ftoi(v.y));
}

extern fixedvec cossinTable[128];

int vectorLength(int x, int y);

inline int distanceTo(int x1, int y1, int x2, int y2)
{
	return vectorLength(x1 - x2, y1 - y2);
}

void precomputeTables();

#endif // LIERO_MATH_HPP
