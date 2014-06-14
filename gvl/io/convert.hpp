#ifndef UUID_EA9A2A74DD9448CF4ABAC285FDC42F3A
#define UUID_EA9A2A74DD9448CF4ABAC285FDC42F3A

#include "../support/debug.hpp"
#include "../support/opt.hpp"
#include "../meta/as_unsigned.hpp"
#include <vector>
#include <string>
#include <cstring>

namespace gvl
{

template<typename DerivedT>
struct basic_text_writer
{
	DerivedT& derived()
	{ return *static_cast<DerivedT*>(this); }
	
	DerivedT const& derived() const
	{ return *static_cast<DerivedT const*>(this); }
};

/*
inline prepared_division const& get_base_divider(int base)
{
	static prepared_division const base_dividers[36-1] =
	{
		prepared_division(2), prepared_division(3),
		prepared_division(4), prepared_division(5),
		prepared_division(6), prepared_division(7),
		prepared_division(8), prepared_division(9),
		prepared_division(10), prepared_division(11),
		prepared_division(12), prepared_division(13),
		prepared_division(14), prepared_division(15),
		prepared_division(16), prepared_division(17),
		prepared_division(18), prepared_division(19),
		prepared_division(20), prepared_division(21),
		prepared_division(22), prepared_division(23),
		prepared_division(24), prepared_division(25),
		prepared_division(26), prepared_division(27),
		prepared_division(28), prepared_division(29),
		prepared_division(30), prepared_division(31),
		prepared_division(32), prepared_division(33),
		prepared_division(34), prepared_division(35),
		prepared_division(36)
	};
	
	sassert(base >= 2 && base <= 36);
	
	return base_dividers[base-2];
}*/

extern uint8_t const no_caps[];
extern uint8_t const caps[];

/*
template<typename Writer, typename T>
int uint_to_ascii(Writer& writer, T x, int base = 10, int min_digits = 1, bool uppercase = false)
{
	if(base < 2 || base > 36)
		return -1;
		
	prepared_division div = get_base_divider(base);
	
	std::size_t const buf_size = sizeof(T) * CHAR_BIT;
	uint8_t digits[buf_size];
	uint8_t* e = digits + buf_size;
	uint8_t* p = e;
	
	uint8_t const* names = uppercase ? caps : no_caps;
	
	while(min_digits-- > 0 || x > 0)
	{
		std::pair<uint32_t, uint32_t> res(div.quot_rem(x));
		*--p = names[res.second];
		x = res.first;
	}
  
	writer.put(p, e - p);
	return 0;
}*/

template<uint32_t Base, typename Writer, typename T>
int uint_to_ascii_base(Writer& writer, T x, int min_digits = 1, bool uppercase = false)
{
	std::size_t const buf_size = sizeof(T) * CHAR_BIT;
	uint8_t digits[buf_size];
	uint8_t* e = digits + buf_size;
	uint8_t* p = e;
	
	uint8_t const* names = uppercase ? caps : no_caps;
	
	while(min_digits-- > 0 || x > 0)
	{
		int n = x % Base;
		*--p = names[n];
		x /= Base;
	}
  
	writer.put(p, e - p);
	return 0;
}

template<uint32_t Base, typename Writer, typename T>
void int_to_ascii_base(Writer& writer, T x, int min_digits = 1, bool uppercase = false)
{
	typedef typename as_unsigned<T>::type unsigned_t;
	
	if(x < 0)
	{
		writer.put('-');
		uint_to_ascii_base<Base>(writer, unsigned_t(-x), min_digits, uppercase);
	}
	else
	{
		uint_to_ascii_base<Base>(writer, unsigned_t(x), min_digits, uppercase);
	}
}
/*
template<typename Writer, typename T>
void int_to_ascii(Writer& writer, T x, int base = 10, int min_digits = 1, bool uppercase = false)
{
	typedef typename as_unsigned<T>::type unsigned_t;
	
	if(x < 0)
	{
		writer.put('-');
		uint_to_ascii(writer, unsigned_t(-x), base, min_digits, uppercase);
	}
	else
	{
		uint_to_ascii(writer, unsigned_t(x), base, min_digits, uppercase);
	}
}*/


template<typename D>
inline D& operator<<(basic_text_writer<D>& self_, uint32_t x)
{
	D& self = self_.derived();
	uint_to_ascii_base<10>(self, x);
	return self;
}

template<typename D>
inline D& operator<<(basic_text_writer<D>& self_, int32_t x)
{
	D& self = self_.derived();
	int_to_ascii_base<10>(self, x);
	return self;
}

template<typename D>
inline D& operator<<(basic_text_writer<D>& self_, char const* str)
{
	D& self = self_.derived();
	self.put(reinterpret_cast<uint8_t const*>(str), std::strlen(str));
	return self;
}

template<typename D>
inline D& operator<<(basic_text_writer<D>& self_, char ch)
{
	D& self = self_.derived();
	self.put(static_cast<uint8_t>(ch));
	return self;
}

template<typename D>
inline D& operator<<(basic_text_writer<D>& self_, std::string const& str)
{
	D& self = self_.derived();
	self.put(reinterpret_cast<uint8_t const*>(str.data()), str.size());
	return self;
}

/*
template<typename D>
inline D& operator<<(basic_text_writer<D>& self_, gvl::string const& str)
{
	D& self = self_.derived();
	self.put(reinterpret_cast<uint8_t const*>(str.data()), str.size());
	return self;
}

template<typename D>
inline D& operator<<(basic_text_writer<D>& self_, void const* ptr)
{
	// TODO: Very TEMP
	return (self_ << uint32_t(ptr));
}*/

#if 0
template<typename T, typename Format>
struct with_format_
{
	with_format_(T const& obj, Format const& f)
	: obj(obj), f(f)
	{
	}

	T const& obj;
	Format const& f;
};

template<typename T, typename Format>
inline with_format_<T, Format> with_f(T const& obj, Format const& f)
{
	return with_format_<T, Format>(obj, f);
}

template<typename D, typename T, typename Format>
inline D& operator<<(basic_text_writer<D>& self_, with_format_<T, Format> wf)
{
	return self_ << wf.f(wf.obj);
}
#endif



struct endl_tag_ {};
inline void endl(endl_tag_) {}

template<typename D>
inline D& operator<<(basic_text_writer<D>& self_, void (*)(endl_tag_))
{
	D& self = self_.derived();
	self.put('\n');
	self.flush();
	return self;
}

struct cell : basic_text_writer<cell>
{
	enum placement
	{
		left, center, right
	};

	cell()
	: text_placement(left)
	, width(-1)
	{
	}

	cell(placement text_placement_init)
	: text_placement(text_placement_init)
	, width(-1)
	{
	}
	
	cell(int width_init, placement text_placement_init)
	: text_placement(text_placement_init)
	, width(width_init)
	{
	}
	
	void put(uint8_t x)
	{ buffer.push_back(x); }
	
	void put(uint8_t const* p, std::size_t len)
	{
		for(std::size_t i = 0; i < len; ++i)
		{
			buffer.push_back(p[i]);
		}
	}

	cell& ref()
	{
		return *this;
	}
	
	std::vector<uint8_t> buffer;
	placement text_placement;
	int width;
};

template<typename D>
inline D& operator<<(basic_text_writer<D>& self_, cell& c)
{
	D& self = self_.derived();
	if(c.buffer.size() > c.width)
	{
		int allowed = std::max(int(c.buffer.size()) - 2, 0);
		self.put(&c.buffer[0], &c.buffer[0] + allowed);
		if(allowed != int(c.buffer.size()))
			self << "..";
	}
	return self;
}

}

#endif // UUID_EA9A2A74DD9448CF4ABAC285FDC42F3A
