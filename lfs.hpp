#ifndef LIERO_LFS_HPP
#define LIERO_LFS_HPP

// Tausworte 1965
template<class UIntType, int w, int k, int q, int s, UIntType val>
struct LFS
{
	typedef unsigned int UIntType;
	typedef UIntType result_type;
	/*
	// avoid the warning trouble when using (1<<w) on 32 bit machines
	BOOST_STATIC_CONSTANT(bool, has_fixed_range = false);
	BOOST_STATIC_CONSTANT(int, word_size = w);
	BOOST_STATIC_CONSTANT(int, exponent1 = k);
	BOOST_STATIC_CONSTANT(int, exponent2 = q);
	BOOST_STATIC_CONSTANT(int, step_size = s);
	*/

	result_type min() const { return 0; }
	result_type max() const { return wordmask; }

	// MSVC 6 and possibly others crash when encountering complicated integral
	// constant expressions.  Avoid the checks for now.
	// BOOST_STATIC_ASSERT(w > 0);
	// BOOST_STATIC_ASSERT(q > 0);
	// BOOST_STATIC_ASSERT(k < w);
	// BOOST_STATIC_ASSERT(0 < 2*q && 2*q < k);
	// BOOST_STATIC_ASSERT(0 < s && s <= k-q);

	explicit LFS(UIntType s0 = 341)
	: wordmask(0)
	{

		for(int i = 0; i < w; ++i)
			wordmask |= (1u << i);
		seed(s0);
	}

/*
	template<class It>
	LFS(It& first, It last)
	: wordmask(0)
	{
		// avoid "left shift count >= with of type" warning
		for(int i = 0; i < w; ++i)
			wordmask |= (1u << i);
		seed(first, last);
	}
*/
	void seed(UIntType s0) 
	{
		assert(s0 >= (1 << (w-k)));
		value = s0;
	}
	
	/*
	template<class It> void seed(It& first, It last)
	{
	if(first == last)
		throw std::invalid_argument("linear_feedback_shift::seed");
	value = *first++;
	assert(value >= (1 << (w-k)));
	}*/

	result_type operator()()
	{
		UIntType const b = (((value << q) ^ value) & wordmask) >> (k-s);
		UIntType const mask = ( (~static_cast<UIntType>(0)) << (w-k) ) & wordmask;
		value = ((value & mask) << s) ^ b;
		return value;
	}
	
	UIntType wordmask; // avoid "left shift count >= width of type" warnings
	UIntType value;
};


#endif // BOOST_RANDOM_LINEAR_FEEDBACK_SHIFT_HPP
