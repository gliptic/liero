#ifndef UUID_64B1EAC4E4F545FA3B131FA346621126
#define UUID_64B1EAC4E4F545FA3B131FA346621126

namespace gvl
{

struct default_delete
{
	template<typename T>
	void operator()(T* p) const
	{
		delete p;
	}
};

struct dummy_delete
{
	template<typename T>
	void operator()(T const&) const
	{
		// Do nothing
	}
};

struct default_compare
{
	template<typename T>
	int operator()(T const& a, T const& b) const
	{
		if(a < b)
			return -1;
		else if(b < a)
			return 1;
		else
			return 0;
	}
};

#if 0

template<typename T>
void caller(void* p)
{
	static_cast<T*>(p)->operator()();
}

struct functor_wrapper
{
	template<typename T>
	functor_wrapper(T& t)
	: f(&caller<T>)
	, p(&t)
	{
		
	}

	void call()
	{ f(p); }

	void(*f)(void* p);
	void* p;
};

#endif

} // namespace gvl

#endif // UUID_64B1EAC4E4F545FA3B131FA346621126
