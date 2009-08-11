#ifndef UUID_A25E9863C68345C3B8E281AB7DFE5401
#define UUID_A25E9863C68345C3B8E281AB7DFE5401

#include <cstddef>
#include <cassert>
#include <gvl/support/debug.hpp>

template<typename T, int Limit>
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
	
	FastObjectList()
	{
		clear();
	}
	
	T* getFreeObject()
	{
		sassert(nextFree != &arr[Limit]);
		T* ptr = nextFree++;
		++count;
		return ptr;
	}
	
	T* newObjectReuse()
	{
		T* ret;
		if(nextFree == &arr[Limit])
			ret = &arr[Limit - 1];
		else
			ret = getFreeObject();

		return ret;
	}
	
	T* newObject()
	{
		if(nextFree == &arr[Limit])
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
		sassert(ptr < nextFree && ptr >= arr);
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
		nextFree = arr;
	}
	
	std::size_t size() const
	{
		return count;
	}
	
	T arr[Limit];
	T* nextFree;
	std::size_t count;
};

#endif // UUID_A25E9863C68345C3B8E281AB7DFE5401
