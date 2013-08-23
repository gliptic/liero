#ifndef UUID_5F7548A068F9485B6759368B5BAE8157
#define UUID_5F7548A068F9485B6759368B5BAE8157

#include <algorithm>

namespace gvl
{

struct weak_ptr_common;

struct shared
{
	friend struct weak_ptr_common;
	
	shared()
	: _ref_count(1), _first(0)
	{

	}
	
	// const to allow shared_ptr<T const>
	void add_ref() const
	{
#if GVL_THREADSAFE
		#error "Not finished"
		// TODO: Interlocked increment
#else
		++_ref_count;
#endif
	}
	
	// const to allow shared_ptr<T const>
	void release() const
	{
#if GVL_THREADSAFE
		#error "Not finished"
		if(_ref_count == 1) // 1 means it has to become 0, nobody can increment it after this read
			_delete();
		else
		{
			// TODO: Implement CAS
			int read_ref_count;
			do
			{
				read_ref_count = _ref_count;
			}
			while(!cas(&_ref_count, read_ref_count, read_ref_count - 1));
			
			if(read_ref_count - 1 == 0)
			{
				_clear_weak_ptrs();
				_delete();
			}
		}
#else
		--_ref_count;
		if(_ref_count == 0)
		{
			_clear_weak_ptrs();
			_delete();
		}
#endif
	}
	
	void swap(shared& b)
	{
		std::swap(_ref_count, b._ref_count);
		std::swap(_first, b._first);
	}
	
	int ref_count() const
	{ return _ref_count; }

	virtual ~shared()
	{
	}
	
private:
	void _delete() const
	{
		delete this;
	}
	
	void _clear_weak_ptrs() const
	{
	}
	
	mutable int _ref_count; // You should be able to have shared_ptr<T const>
	weak_ptr_common* _first;
};

} // namespace gvl

#endif // UUID_5F7548A068F9485B6759368B5BAE8157
