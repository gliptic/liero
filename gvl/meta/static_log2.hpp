#ifndef UUID_B1A45C6C76DE4AE4E7817095BE18B516
#define UUID_B1A45C6C76DE4AE4E7817095BE18B516

#include "../support/cstdint.hpp"

namespace gvl
{

template<uint64_t X, int M = (1 + GVL_BITS_IN(uint64_t))/2>
struct static_log2_impl
{
	static int const c = (X >> M) > 0;
	static uint64_t const value = c*M + static_log2_impl<(X >> (c*M)), M/2>::value;
};

template<>
struct static_log2_impl<1,0>
{ enum { value = 0 }; };


template<uint64_t X>
struct static_log2
{
	static uint64_t const value = static_log2_impl<X>::value;
};

template<>
struct static_log2<0>
{};


}

#endif // UUID_B1A45C6C76DE4AE4E7817095BE18B516
