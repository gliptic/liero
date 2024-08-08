#ifndef UUID_038DE51568F04236FF21B1A4AFF43D3C
#define UUID_038DE51568F04236FF21B1A4AFF43D3C

#include <cassert>
#include <algorithm>
#include "shared.hpp"
#include "../support/debug.hpp"
#include "../support/functional.hpp"
//#include "shared_count.hpp"

namespace gvl
{

struct weak_ptr_common;

struct shared_ownership {};

template<typename T>
struct deferred_ptr;

/*
template<typename T>
struct unsafe_weak_ptr;*/

template<typename T>
struct shared_ptr // : shared_ptr_common
{
	shared_ptr()
	: v(0)
	{ }

	// Takes ownership, v_init assumed fresh (no add_ref!)
	explicit shared_ptr(T* v_init)
	: v(v_init)
	{
	}

	// Shares ownership
	explicit shared_ptr(T* v_init, shared_ownership)
	{
		_set(v_init);
	}

	~shared_ptr()
	{
		_release();
	}

	shared_ptr(shared_ptr const& b)
	{
		_set(b.get());
	}

	template<typename SrcT>
	shared_ptr(shared_ptr<SrcT> const& b)
	{
		T* p = b.get();
		_set(p);
	}

#if GVL_CPP0X
	shared_ptr(shared_ptr&& b)
	{
		v = b.get();
		b.v = 0;
	}

	template<typename SrcT>
	friend struct shared_ptr;

	template<typename SrcT>
	shared_ptr(shared_ptr<SrcT>&& b)
	{
		T* p = b.get();
		v = p;
		b.v = 0;
	}
#endif

	// These two take over reference from b
	shared_ptr(deferred_ptr<T> const& b);
	shared_ptr& operator=(deferred_ptr<T> const& b);

	shared_ptr& operator=(shared_ptr const& b)
	{
		_reset_shared(b.get());
		return *this;
	}

	template<typename SrcT>
	shared_ptr& operator=(shared_ptr<SrcT> const& b)
	{
		T* p = b.get();
		_reset_shared(p);
		return *this;
	}

#if GVL_CPP0X
	shared_ptr& operator=(shared_ptr&& b)
	{
		_reset_shared(b.get());
		b.v = 0;
		return *this;
	}

	template<typename SrcT>
	shared_ptr& operator=(shared_ptr<SrcT>&& b)
	{
		T* p = b.get();
		_reset_shared(p);
		b.v = 0;
		return *this;
	}
#endif

	operator void const*() const
	{ return v; }

	T* operator->() const
	{ sassert(v); return static_cast<T*>(v); }

	T& operator*() const
	{ sassert(v); return *static_cast<T*>(v); }

	// Takes ownership, v_new assumed fresh (no add_ref!)
	void reset(T* v_new)
	{ _reset(v_new); }

	void reset(T* v_new, shared_ownership)
	{
		_reset_shared(v_new);
	}

	void reset()
	{ _release(); v = 0; }

	shared_ptr release()
	{
		shared_ptr ret;
		ret.v = v;
		v = 0;
		return ret;
	}

	void swap(shared_ptr& b)
	{ std::swap(v, b.v); }

	template<typename DestT>
	shared_ptr<DestT> cast()
	{ return shared_ptr<DestT>(dynamic_cast<DestT*>(get()), shared_ownership()); }

	template<typename DestT>
	shared_ptr<DestT> static_cast_()
	{ return shared_ptr<DestT>(static_cast<DestT*>(get()), shared_ownership());	}

	T& cow()
	{
		sassert(v);
		if(v->ref_count() > 1)
			reset(get()->clone());
		return *get();
	}

	T* get() const
	{ return static_cast<T*>(v); }

private:
	// Takes ownership (no add_ref!)
	void _reset(T* v_new)
	{
		sassert(v_new != v); // self-reset is invalid
		_release();
		v = v_new;
	}

	// Shares ownership
	void _reset_shared(T* v_new)
	{
		T* old = v;
		_set(v_new);
		if(old)
			old->release();
	}

	void _release()
	{
		if(v)
			v->release();
	}

	void _set(T* v_new)
	{
		v = v_new;
		if(v)
			v->add_ref();
	}

	void _set_non_zero(T* v_new)
	{
		v = v_new;
		sassert(v);
		v->add_ref();
	}

	T* v;
};

struct deferred_ptr_raw_ptr_ {};

// Cheaper, ownership-passing version of shared_ptr
template<typename T>
struct deferred_ptr // : shared_ptr_common
{
private:

public:
	template<typename T2>
	friend struct shared_ptr;

	deferred_ptr()
	: v(0)
	{ }

	// Takes ownership, v_init assumed fresh (no add_ref!)
	explicit deferred_ptr(T* v_init)
	: v(v_init)
	{
	}

	// Shares ownership
	explicit deferred_ptr(T* v_init, shared_ownership)
	{
		_set(v_init);
	}

	~deferred_ptr()
	{
		_release();
	}

	template<typename SrcT>
	deferred_ptr(shared_ptr<SrcT> const& b)
	{
		T* p = b.get();
		_set(p);
	}

	// Takes over reference from b
	template<typename SrcT>
	deferred_ptr(deferred_ptr<SrcT> const& b)
	{
		v = b.get();
		b.v = 0;
	}

	// Takes over reference from b
	deferred_ptr(deferred_ptr const& b)
	{
		v = b.get();
		b.v = 0;
	}

	template<typename SrcT>
	deferred_ptr& operator=(shared_ptr<SrcT> const& b)
	{
		T* p = b.get();
		_reset_shared(p);
		return *this;
	}

	// Takes over reference from b
	template<typename SrcT>
	deferred_ptr& operator=(deferred_ptr<SrcT> const& b)
	{
		v = b.get();
		b.v = 0;
		return *this;
	}

	// Takes over reference from b
	deferred_ptr& operator=(deferred_ptr const& b)
	{
		v = b.get();
		b.v = 0;
		return *this;
	}

	operator void const*() const
	{ return v;	}

	T* operator->() const
	{ sassert(v); return v; }

	T& operator*() const
	{ assert(v); return *v;	}

	// Takes ownership, v_new assumed fresh (no add_ref!)
	void reset(T* v_new)
	{ _reset(v_new); }

	void reset()
	{ _release(); v = 0; }

	void swap(deferred_ptr& b)
	{ std::swap(v, b.v); }

	template<typename DestT>
	deferred_ptr<DestT> cast()
	{
		deferred_ptr<DestT> ret(dynamic_cast<DestT*>(get()));
		v = 0;
		return ret;
	}

	template<typename DestT>
	deferred_ptr<DestT> static_cast_()
	{
		deferred_ptr<DestT> ret(static_cast<DestT*>(get()));
		v = 0;
		return ret;
	}

	T* get() const
	{ return static_cast<T*>(v); }

private:


	void _reset(T* v_new)
	{
		sassert(v_new != v); // self-reset is invalid
		_release();
		v = v_new;
	}

	void _reset_shared(T* v_new)
	{
		T* old = v; // Handles self-reset.
		_set(v_new);
		if(old)
			old->release();
	}

	void _release()
	{
		if(v)
			v->release();
	}

	void _set(T* v_new)
	{
		v = v_new;
		if(v)
			v->add_ref();
	}

	void _set_non_zero(T* v_new)
	{
		v = v_new;
		sassert(v);
		v->add_ref();
	}

	mutable T* v;
};

template<typename T>
deferred_ptr<T> share_ownership(T* ptr)
{ return deferred_ptr<T>(ptr, shared_ownership()); }

/*
// Like a normal pointer, but ownership is assumed to belong
// to something else.
template<typename T>
struct unsafe_weak_ptr : shared_ptr_common
{
	unsafe_weak_ptr()
	{ }

	// Does not take ownership
	explicit unsafe_weak_ptr(T* v)
	: shared_ptr_common(v)
	{ }

	template<typename SrcT>
	unsafe_weak_ptr(shared_ptr<SrcT> const& b)
	: shared_ptr_common(b.get())
	{ }

	template<typename SrcT>
	unsafe_weak_ptr(unsafe_weak_ptr<SrcT> const& b)
	: shared_ptr_common(b.get())
	{ }

	unsafe_weak_ptr(unsafe_weak_ptr const& b)
	{
		v = b.get();
	}

	template<typename SrcT>
	unsafe_weak_ptr& operator=(shared_ptr<SrcT> const& b)
	{
		v = b.get();
		return *this;
	}

	template<typename SrcT>
	unsafe_weak_ptr& operator=(unsafe_weak_ptr<SrcT> const& b)
	{
		v = b.get();
		return *this;
	}

	unsafe_weak_ptr& operator=(unsafe_weak_ptr const& b)
	{
		v = b.get();
		return *this;
	}

	operator void*() const
	{ return v;	}

	T* operator->() const
	{ sassert(v); return v; }

	T& operator*() const
	{ assert(v); return *v;	}

	void reset()
	{ v = 0; }

	void swap(unsafe_weak_ptr& b)
	{ std::swap(v, b.v); }

	template<typename DestT>
	unsafe_weak_ptr<DestT> cast()
	{ return dynamic_cast<DestT*>(get()); }

	T* get() const
	{ return static_cast<T*>(v); }
};*/

template<typename T>
shared_ptr<T>::shared_ptr(deferred_ptr<T> const& b)
{
	v = b.v;
	b.v = 0;
}

template<typename T>
shared_ptr<T>& shared_ptr<T>::operator=(deferred_ptr<T> const& b)
{
	_release();
	v = b.v;
	b.v = 0;
	return *this;
}

template<typename T>
shared_ptr<T> make_shared(T* ptr)
{
	return shared_ptr<T>(ptr);
}

} // namespace gvl


#endif // UUID_038DE51568F04236FF21B1A4AFF43D3C
