#ifndef GVL_GASH_HPP
#define GVL_GASH_HPP

#include "../support/cstdint.hpp"
#include <utility>

namespace gvl
{

inline uint64_t rot(uint64_t v, int c)
{
	return (v << c) | (v >> (64-c));
}

template<int Length>
struct hash_value
{
	static int const size = Length;
	
	uint64_t value[Length];
	
	hash_value()
	{
		for(int i = 0; i < Length; ++i)
			value[i] = 0;
	}
	
	bool operator!=(hash_value const& b) const
	{
		for(int i = 0; i < Length; ++i)
		{
			if(value[i] != b.value[i])
				return true;
		}
		return false;
	}
	
	bool operator==(hash_value const& b) const
	{
		return !operator!=(b);
	}
};

struct gash
{
	static int const block_size = 4;
	
	typedef hash_value<block_size> value_type;

	gash()
	{
		uint64_t accum = 1;
		for(int i = 0; i < 8; ++i)
		{
			accum *= 0x579d16a0ull;
			accum += 1;
			d[i] = accum;
		}
	}
	
	void process(uint64_t* n)
	{
		d[0] ^= n[0];
		d[1] ^= n[1];
		d[2] ^= n[2];
		d[3] ^= n[3];
		
		for(int i = 0; i < 4; ++i)
		{
			round1();
			round2();
		}
		
		d[4] += n[0];
		d[5] += n[1];
		d[6] += n[2];
		d[7] += n[3];
	}
	
	void round1()
	{
		d[0] -= d[5];
		d[1] -= d[6];
		d[2] -= d[7];
		d[3] ^= d[0];
		d[4] ^= d[1];
		d[5] ^= d[2];
		
		d[6] = rot(d[6], 17);
		d[7] = rot(d[7], 37);
		
		std::swap(d[0], d[4]);
		std::swap(d[2], d[5]);
		std::swap(d[3], d[7]);
	}
	

	void round2()
	{
		d[7] -= d[2];
		d[6] -= d[1];
		d[5] -= d[0];
		d[4] ^= d[7];
		d[3] ^= d[6];
		d[2] ^= d[5];
		
		d[0] = rot(d[0], 13);
		d[1] = rot(d[1], 23);
		
		std::swap(d[1], d[2]);
		std::swap(d[3], d[5]);
		std::swap(d[7], d[0]);
	}

	value_type final() const
	{
		value_type ret;
		for(int i = 0; i < block_size; ++i)
		{
			ret.value[i] = d[i];
		}
		return ret;
	}
	
	uint64_t d[8];
};

template<typename Hash>
struct hash_accumulator
{
	void put(uint8_t v)
	{
		bit_n -= 8;
		cur |= (uint64_t(v) << bit_n);
		if(bit_n == 0)
		{
			dump_cur();
		}
	}
	
	void put(uint8_t const* p, std::size_t len)
	{
		for(std::size_t i = 0; i < len; ++i)
			put(p[i]);
	}
	
	void dump_cur()
	{
		bit_n = 64;
		buf[word_n++] = cur;
		cur = 0;
		if(word_n == Hash::block_size)
		{
			hash_.process(buf);
			word_n = 0;
		}
	}
	
#if 0 // Untested
	void ui32(uint32_t v)
	{
		if(bit_n >= 32)
		{
			bit_n -= 32;
			cur |= (uint64_t(v) << bit_n);
			if(bit_n == 0)
				dump_cur();
		}
		else
		{
			int left = bit_n;
			cur |= v >> (32 - left);
			dump_cur();
			bit_n = 64 - (32 - left);
			cur = uint64_t(v) << bit_n;
		}
	}
#endif
	
	void flush()
	{
		// Pad with one followed by zeroes
		put(0x80);

		// Flush cur
		if(bit_n < 64)
		{
			buf[word_n++] = cur;
		}
		
		// Flush buf
		if(word_n > 0)
		{
			// Pad with 0
			for(int i = word_n; i < Hash::block_size; ++i)
			{
				buf[i] = 0;
			}
			hash_.process(buf);
		}
		
		bit_n = 64;
		word_n = 0;
		cur = 0;
	}
	
	typename Hash::value_type final() const
	{
		return hash_.final();
	}
	
	hash_accumulator()
	: bit_n(64)
	, word_n(0)
	, cur(0)
	{
	}
	
	Hash& hash()
	{
		return hash_;
	}
	
	Hash hash_;
	uint64_t buf[Hash::block_size];
	int bit_n;
	int word_n;
	uint64_t cur;
};

}

#endif // GVL_GASH_HPP
