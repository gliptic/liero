#ifndef UUID_A25E9863C68345C3B8E281AB7DFE5401
#define UUID_A25E9863C68345C3B8E281AB7DFE5401

#include <cstddef>
#include <cassert>
#include <vector>
#include <algorithm>
#include <gvl/support/debug.hpp>


template<typename T>
struct FastObjectList
{
	struct iterator
	{
		iterator(T* cur_)
		: cur(cur_)
		{
		}
		
		iterator& operator++()
		{
			++cur;			
			return *this;
		}
		
		T& operator*()
		{
			return *cur;
		}
		
		T* operator->()
		{
			return cur;
		}
		
		bool operator!=(iterator b)
		{
			return cur != b.cur;
		}
		
		T* cur;
	};
	
	FastObjectList(std::size_t limit = 1)
	: limit(limit), arr(limit)
	{
		clear();
	}
	
	T* getFreeObject()
	{
		sassert(nextFree != &arr[0] + limit);
		T* ptr = nextFree++;
		++count;
		return ptr;
	}
	
	T* newObjectReuse()
	{
		T* ret;
		if(nextFree == &arr[0] + limit)
			ret = &arr[limit - 1];
		else
			ret = getFreeObject();

		return ret;
	}
	
	T* newObject()
	{
		if(nextFree == &arr[0] + limit)
			return 0;
			
		T* ret = getFreeObject();
		return ret;
	}
	
	iterator begin()
	{
		return iterator(&arr[0]);
	}
	
	iterator end()
	{
		return iterator(nextFree);
	}
		
	void free(T* ptr)
	{
		sassert(ptr < nextFree && ptr >= &arr[0]);
		*ptr = *--nextFree;
		--count;
	}
	
	void free(iterator i)
	{
		free(&*i);
	}
	
	void clear()
	{
		count = 0;
		nextFree = &arr[0];
	}
	
	void resize(std::size_t newLimit)
	{
		limit = newLimit;
		count = std::min(count, newLimit);
		arr.resize(newLimit);
		nextFree = &arr[0] + count;
	}
	
	std::size_t size() const
	{
		return count;
	}
	
	std::size_t limit;
	std::vector<T> arr;
	T* nextFree;
	std::size_t count;
};

#endif // UUID_A25E9863C68345C3B8E281AB7DFE5401
