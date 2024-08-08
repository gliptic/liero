#ifndef LIERO_EXACTOBJECTLIST_HPP
#define LIERO_EXACTOBJECTLIST_HPP

#include <cstddef>
#include <cassert>
#include <gvl/support/bits.hpp>
#include <cstring>

struct ExactObjectListBase
{
	bool used;
};

template<typename T, int Limit>
struct ExactObjectList
{
#if 0
	struct iterator
	{
		iterator(T* cur_)
		: cur(cur_)
		{
			while(!cur->used)
			{
				++cur;
			}
		}

		iterator& operator++()
		{
			do
			{
				++cur;
			}
			while(!cur->used);

			return *this;
		}

		T& operator*()
		{
			assert(cur->used);
			return *cur;
		}

		T* operator->()
		{
			assert(cur->used);
			return cur;
		}

		bool operator!=(iterator b)
		{
			return cur != b.cur;
		}

		T* cur;
	};
#endif

	struct range
	{
		range(T* cur, T* end)
		: cur(cur), end(end)
		{
		}

		T* next()
		{
			while (!cur->used)
				++cur;

			T* ret = cur;
			++cur;

			return ret == end ? 0 : ret;
		}

		T* cur;
		T* end;
	};

	ExactObjectList()
	{
		clear();
	}

	T* getFreeObject()
	{
		assert(count < Limit);
		++count;

		T* ptr = 0;
		for (uint32_t i = 0; i < FreeListSize; ++i)
		{
			if (freeList[i] != 0)
			{
				int bit = gvl_bottom_bit(freeList[i]);
				uint32_t index = (i << 5) + bit;
				ptr = arr + index;
				freeList[i] &= ~(uint32_t(1) << bit);
				break;
			}
		}

		assert(ptr && !ptr->used && ptr >= arr && ptr < arr + Limit);
		ptr->used = true;

		return ptr;
	}

	T* newObjectReuse()
	{
		T* ret;
		if(count >= Limit)
			ret = &arr[Limit - 1];
		else
			ret = getFreeObject();

		assert(ret->used && ret >= arr && ret < arr + Limit);
		return ret;
	}

	T* newObject()
	{
		if(count >= Limit)
			return 0;

		T* ret = getFreeObject();
		assert(ret->used && ret >= arr && ret < arr + Limit);
		return ret;
	}

#if 0
	iterator begin()
	{
		return iterator(&arr[0]);
	}

	iterator end()
	{
		return iterator(&arr[Limit]);
	}
#endif

	range all()
	{
		return range(&arr[0], &arr[Limit]);
	}

#if 1
	void free(T* ptr)
	{
		assert(ptr->used);
		if(ptr->used)
		{
			uint32_t index = uint32_t(ptr - arr);
			freeList[index >> 5] |= (uint32_t(1) << (index & 31));

			ptr->used = false;

			assert(count > 0);
			--count;
		}
	}
#endif

	void free(range& r)
	{
		free(r.cur - 1);
	}

	void clear()
	{
		std::memset(freeList, 0xff, FreeListSize * sizeof(uint32_t));
		count = 0;

		for(std::size_t i = 0; i < Limit; ++i)
			arr[i].used = false;

		arr[Limit].used = true;

		// Mark padding as used
		for(uint32_t index = Limit; index < FreeListSize * 32; ++index)
			freeList[index >> 5] &= ~(uint32_t(1) << (index & 31));
	}

	std::size_t size() const
	{
		return count;
	}

	T arr[Limit + 1]; // Sentinel

	static uint32_t const FreeListSize = (Limit + 31) / 32;
	uint32_t freeList[FreeListSize];

	std::size_t count;
};

#endif // LIERO_EXACTOBJECTLIST_HPP
