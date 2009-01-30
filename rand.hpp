#ifndef LIERO_RAND_HPP
#define LIERO_RAND_HPP

#include <SDL/SDL.h>

struct Rand
{
	// Currently the LCG used in Super-Duper
	static Uint32 const A = 69069; // 1664525;
	static Uint32 const B = 1; // 1013904223;
	
	Rand()
	: curSeed(0)
	{
	}
	
	void seed(Uint32 newSeed)
	{
		curSeed = (newSeed * 2654435761UL) & 0xffffffffUL;
	}
	
	Uint32 next()
	{
        curSeed = (A * curSeed + B) & 0xffffffff;
        return curSeed;
	}
	
	Uint32 operator()()
	{
		return next();
	}
	
	Uint32 operator()(Uint32 max)
	{
		Uint64 v = next();
		v *= max;
		return Uint32(v >> 32);
	}
	
	Uint32 operator()(Uint32 min, Uint32 max)
	{
		return (*this)(max - min) + min;
	}
	
	Uint32 curSeed;
};

#endif // LIERO_RAND_HPP
