#pragma once

#include "math/rect.hpp"

using fixedvec = IVec2;

typedef int fixed;

inline fixed itof(int v) { return v << 16; }

inline int ftoi(fixed v) { return v >> 16; }

inline fixedvec itof(IVec2 v) { return fixedvec(itof(v.x), itof(v.y)); }

inline IVec2 ftoi(fixedvec v) { return IVec2(ftoi(v.x), ftoi(v.y)); }

extern fixedvec cossinTable[128];

int vectorLength(int x, int y);

inline int distanceTo(int x1, int y1, int x2, int y2) { return vectorLength(x1 - x2, y1 - y2); }

void precomputeTables();
