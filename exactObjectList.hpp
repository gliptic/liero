#ifndef LIERO_EXACTOBJECTLIST_HPP
#define LIERO_EXACTOBJECTLIST_HPP

#include <cstddef>
#include <cassert>
#include <gvl/support/bits.hpp>

#if 0
#include <gvl/containers/pairing_heap.hpp>

struct ExactObjectListBase2 : gvl::pairing_node<>
{
	bool operator<(ExactObjectListBase2 const& b) const
	{
		return index < b.index;
	}
	
	int index;
	bool used;
};

template<typename T, int Limit>
struct ExactObjectList2
{
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
	
	ExactObjectList2()
	{
		clear();
	}
	
	T* getFreeObject()
	{
		/*
		T* ptr = static_cast<T*>(nextFree);
		nextFree = ptr->nextFree;
		*/
		assert(!freeList.empty());
		T* ptr = freeList.unlink_min();
		assert(!ptr->used);
		ptr->used = true;
		
		/*
		sentinel.prev->next = ptr;
		ptr->prev = sentinel.prev;
		ptr->next = &sentinel;
		sentinel.prev = ptr;
		*/
		
		++count;
		
		return ptr;
	}
	
	T* newObjectReuse()
	{
		T* ret;
		if(freeList.empty())
			ret = &arr[Limit - 1];
		else
			ret = getFreeObject();
			
		assert(ret->used);
		return ret;
	}
	
	T* newObject()
	{
		if(freeList.empty())
			return 0;
			
		T* ret = getFreeObject();
		assert(ret->used);
		return ret;
	}
	
	iterator begin()
	{
		return iterator(&arr[0]);
	}
	
	iterator end()
	{
		return iterator(&arr[Limit]);
	}
		
	void free(T* ptr)
	{
		assert(ptr->used);
		if(ptr->used)
		{
			//ptr->nextFree = nextFree;
			//nextFree = ptr;
			freeList.insert(ptr);
			ptr->used = false;
			
			assert(count > 0);
			
			--count;
		}
	}
	
	void free(iterator i)
	{
		free(&*i);
	}
	
	void clear()
	{
		count = 0;
		/*
		sentinel.prev = &sentinel;
		sentinel.next = &sentinel;*/
		
		arr[Limit].used = true; // Sentinel
		
		for(std::size_t i = 0; i < Limit; ++i)
		{
			//arr[i].nextFree = &arr[i + 1];
			arr[i].used = false;
			arr[i].index = i;
			freeList.insert(&arr[i]);
		}
		
		//arr[Limit - 1].nextFree = 0;
		//arr[Limit - 1].used = false;
		//nextFree = &arr[0];
	}
	
	std::size_t size() const
	{
		return count;
	}
	
	T arr[Limit + 1]; // One sentinel

	//ObjectListBase* nextFree;
	//ObjectListBase sentinel;
	gvl::pairing_heap<T, gvl::default_pairing_tag, std::less<T>, gvl::dummy_delete> freeList;
	std::size_t count;
};

#endif

struct ExactObjectListBase
{
	bool used;
};

template<typename T, int Limit>
struct ExactObjectList
{
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

		assert(ptr && !ptr->used);
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
			
		assert(ret->used);
		return ret;
	}
	
	T* newObject()
	{
		if(count >= Limit)
			return 0;
			
		T* ret = getFreeObject();
		assert(ret->used);
		return ret;
	}
	
	iterator begin()
	{
		return iterator(&arr[0]);
	}
	
	iterator end()
	{
		return iterator(&arr[Limit]);
	}
		
	void free(T* ptr)
	{
		assert(ptr->used);
		if(ptr->used)
		{
			uint32_t index = (ptr - arr);
			freeList[index >> 5] |= (uint32_t(1) << (index & 31));

			ptr->used = false;
			
			assert(count > 0);
			--count;
		}
	}
	
	void free(iterator i)
	{
		free(&*i);
	}
	
	void clear()
	{
		std::memset(freeList, 0xff, FreeListSize * sizeof(uint32_t));
		count = 0;
		
		for(std::size_t i = 0; i < Limit; ++i)
			arr[i].used = false;

		// Mark padding as used
		for(uint32_t index = Limit; index < FreeListSize * 32; ++index)
			freeList[index >> 5] &= ~(uint32_t(1) << (index & 31));
	}
	
	std::size_t size() const
	{
		return count;
	}
	
	T arr[Limit];

	static uint32_t const FreeListSize = (Limit + 31) / 32;
	uint32_t freeList[FreeListSize];

	std::size_t count;
};

#endif // LIERO_EXACTOBJECTLIST_HPP
