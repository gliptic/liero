#pragma once

#include "math/rect.hpp"

using fixedvec = IVec2;

using fixed = int;

inline fixed Itof(int v) { return v << 16; }

inline int Ftoi(fixed v) { return v >> 16; }

inline fixedvec Itof(IVec2 v) { return {Itof(v.x), Itof(v.y)}; }

inline IVec2 Ftoi(fixedvec v) { return {Ftoi(v.x), Ftoi(v.y)}; }

extern fixedvec cossin_table[128];

int VectorLength(int x, int y);

inline int DistanceTo(int x1, int y1, int x2, int y2) { return VectorLength(x1 - x2, y1 - y2); }

void PrecomputeTables();
