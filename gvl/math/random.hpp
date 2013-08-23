#ifndef UUID_F29926F3240844A09DCFB9B1828C7DC8
#define UUID_F29926F3240844A09DCFB9B1828C7DC8

#include "../support/debug.hpp"

namespace gvl
{

template<typename DerivedT, typename ValueT>
struct prng_common
{
	typedef ValueT value_type;
	
	DerivedT& derived()
	{ return *static_cast<DerivedT*>(this); }
	
	// Number in [0.0, 1.0)
	double get_double()
	{
		uint32_t v = derived()();
		// This result should be exact if at least double-precision is used. Therefore
		// there shouldn't be any reason to use gD.
		double ret = v / 4294967296.0;
		return ret;
	}
	
	// NOTE! Not reproducible right now. We don't want
	// to take the (potential) hit if it's not necessary.
	// Number in [0.0, max)
	double get_double(double max)
	{
		return get_double() * max;
	}
	
	// Number in [0, max)
	uint32_t operator()(uint32_t max)
	{
		uint64_t v = derived()();
		v *= max;
		return uint32_t(v >> 32);
	}
	
	// Number in [min, max)
	uint32_t operator()(uint32_t min, uint32_t max)
	{
		sassert(min < max);
		return operator()(max - min) + min;
	}
};

} // namespace gvl

#endif // UUID_F29926F3240844A09DCFB9B1828C7DC8

