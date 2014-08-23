#ifndef GVL_SERIALIZATION_TOML_HPP
#define GVL_SERIALIZATION_TOML_HPP

#include "../io2/convert.hpp"

namespace gvl
{

namespace toml
{

template<typename Writer>
struct writer
{
	writer(Writer& w)
	: w(w), inObject(false)
	, indent(0)
	{
	}

	void windent()
	{
		w << '\n';
		for (int i = 0; i < indent; ++i)
		{
			w << "  ";
		}
	}

	int indent;
	std::vector<char const*> chain;
	bool inObject;

	void beginf(char const* name)
	{
		chain.push_back(name);
		inObject = false;
	}

	void fname()
	{
		bool first = true;
		for (std::size_t i = 0; i < chain.size(); ++i)
		{
			if (first)
			{
				first = false;
			}
			else
				w << '.';
			w << chain[i];
		}
	}

	void f(char const* name)
	{
		if (name)
		{
			if (!inObject)
			{
				if (!chain.empty())
				{
					w << '\n';
					windent();
					w << "[";
					fname();
					w << "]\n";
				}
				inObject = true;
			}
			windent();
			w << name << " = ";
		}
	}

	void endf()
	{
		chain.pop_back();
		inObject = false;
	}

	template<typename F>
	void obj(char const* name, F func)
	{
		if (name)
			beginf(name);

		func();
		
		if (name)
			endf();
	}

	template<typename A, typename F>
	void arr(char const* name, A& arr, F func)
	{
		f(name);
		w << '[';
		bool first = true;
		for (auto& e : arr)
		{
			if (first)
				first = false;
			else
				w << ", ";
			func(e);
		}
		w << "]";
	}

	template<typename A, typename F>
	void array_obj(char const* name, A& arr, F func)
	{
		beginf(name);

		for (auto& e : arr)
		{
			w << '\n';
			windent();
			w << "[[";
			fname();
			w << "]]\n";
			++indent;
			inObject = true;
			
			func(e);
			--indent;
		}

		endf();
	}

	void i32(int v)
	{
		w << v;
	}

	void i32(char const* name, int v)
	{
		f(name);
		i32(v);
	}

	void b(bool v)
	{
		w << (v ? "true" : "false");
	}

	void b(char const* name, bool v)
	{
		f(name);
		b(v);
	}

	void null(char const* name)
	{
		f(name);
		w << "null";
	}

	template<typename T, typename Resolver>
	void ref(char const* name, T const& v, Resolver resolver)
	{
		f(name);
		resolver.v2r(*this, v);
	}

	void str(std::string& s)
	{
		w << '"';
		for (char c : s)
		{
			if (c >= 0x20 && c <= 0x7e)
			{
				if (c == '"' || c == '\\')
					w << '\\';
				w << c;
			}
			else
			{
				w << "\\u";
				gvl::uint_to_ascii_base<16>(w, (uint8_t)c, 4);
			}
			// TODO: Handle non-printable
		}
		w << '"';
	}

	void str(char const* name, std::string& s)
	{
		f(name);
		str(s);
	}

	Writer& w;
};

enum type
{
	t_null,
	t_bool,
	t_integer,
	t_string,
	t_object,
	t_array
};

struct object;
struct array;
struct string;

using gvl::shared;
using std::vector;
using std::map;
using std::move;

struct value
{
	type tt;
	union
	{
		int i;
		shared* s;
	} u;

	value()
	: tt(t_null)
	{
	}

	value(int v)
	: tt(t_integer)
	{
		u.i = v;
	}

	value(bool v)
	: tt(t_bool)
	{
		u.i = v;
	}

	value(value const& other)
	: tt(other.tt), u(other.u)
	{
		if (tt >= t_string)
			u.s->add_ref();
	}

	value(value&& other)
	: tt(other.tt), u(other.u)
	{
		other.tt = t_null;
	}

	inline value(object&& o);
	inline value(array&& a);
	inline value(string&& s);

	value& operator=(value const& other)
	{
		shared* old = tt >= t_string ? u.s : 0;
		tt = other.tt;
		u = other.u;
		if (tt >= t_string)
			u.s->add_ref();
		if (old) old->release();
		return *this;
	}

	value& operator=(value&& other)
	{
		shared* old = tt >= t_string ? u.s : 0;
		tt = other.tt;
		u = other.u;
		other.tt = t_null;
		if (old) old->release();
		return *this;
	}

	object* as_object()
	{
		if (tt != t_object)
			return 0;
		return (object*)u.s;
	}

	array* as_array()
	{
		if (tt != t_array)
			return 0;
		return (array*)u.s;
	}

	~value()
	{
		if (tt >= t_string)
			u.s->release();
	}
};

struct string : shared
{
	std::string s;
};

struct array : shared
{
	vector<value> v;
};

struct object : shared
{
	map<std::string, value> f;
};

inline value::value(object&& o)
: tt(t_object)
{
	u.s = new object(o);
}

inline value::value(array&& a)
: tt(t_array)
{
	u.s = new array(a);
}

inline value::value(string&& s)
: tt(t_string)
{
	u.s = new string(s);
}

struct parse_error
{
};

template<typename T>
inline void resize(vector<T>& v, std::size_t s)
{
	v.resize(s);
}

template<typename T, int N>
inline void resize(T(&arr)[N], std::size_t s)
{
	if (N != s)
	{
		throw parse_error();
	}
}

template<typename Reader>
struct reader
{
	reader(Reader& r)
	: r(r)
	, root(object())
	{
		start();
	}

	value root, cur;

	value find(value const& o, vector<std::string> const& path, bool makeArray, bool assign = false)
	{
		value c = o;

		size_t count = path.size();

		if (assign)
			--count;

		for (size_t i = 0; i < count; ++i)
		{
			std::string const& e = path[i];

			if (object* obj = c.as_object())
			{
				value& v = obj->f[e];
				if (v.tt == t_null)
				{
					if (makeArray && i == count - 1)
					{
						array a;
						a.v.emplace_back(object());
						c = a.v.back();
						v = value(move(a));
					}
					else
					{
						c = v = value(object());
					}
				}
				else
				{
					c = v;

					while (array* a = c.as_array())
					{
						if (makeArray && i == count - 1)
						{
							a->v.emplace_back(object());
						}
						else if (a->v.empty())
							throw parse_error();

						c = a->v.back();
					}

					if (c.tt != t_object)
					{
						throw parse_error();
					}
				}
			}
			else
			{
				throw parse_error();
			}
		}

		return c;
	}

	void start()
	{
		next();
		cur = root;

		do
		{
			skipws();
			if (c == '[')
			{
				bool isArrayObj = false;
				next();
				if (c == '[')
				{
					next();
					isArrayObj = true;
				}

				auto name(dotted());

				check(']');

				if (isArrayObj)
				{
					check(']');
				}

				cur = find(root, name, isArrayObj);
			}
			else if (c == '\n' || c == '\r')
			{
				// Empty line
			}
			else
			{
				auto name(dotted());
				skipws();

				check('=');
				skipws();

				auto v(val());
				skipws();

				value o(find(cur, name, false, true));

				if (object* obj = o.as_object())
				{
					obj->f.insert(std::make_pair(name.back(), v));
				}
				else
				{
					throw parse_error();
				}
			}
		}
		while (test('\n'));

		cur = root;

		check(0);
	}

	vector<std::string> dotted()
	{
		vector<std::string> ret(1);

		for (;;)
		{
			if ((c >= 'a' && c <= 'z')
			 || (c >= 'A' && c <= 'Z')
			 || (c >= '0' && c <= '9')
			 || (c == '_'))
			{
				ret.back().push_back(c);
				next();
			}
			else if (c == '.')
			{
				ret.push_back(std::string());
				next();
			}
			else
			{
				break;
			}
		}

		return std::move(ret);
	}

	void skipws()
	{
		while (c == '\t' || c == ' ')
		{
			next();
		}
	}

	value f(char const* name)
	{
		if (name)
		{
			if (cur.tt != t_object)
				throw parse_error();
			return ((object*)cur.u.s)->f.at(name);
		}
		else
		{
			return cur;
		}
	}

	template<typename F>
	void obj(char const* name, F func)
	{
		value parent(cur);
		cur = f(name);
		func();
		cur = move(parent);
	}

	template<typename A, typename F>
	void arr(char const* name, A& arr, F func)
	{
		value parent(cur);
		value v(f(name));
		if (v.tt != t_array)
			throw parse_error();
		array& jv = *(array*)v.u.s;
		resize(arr, jv.v.size());

		auto i = jv.v.begin();
		for (auto& e : arr)
		{
			cur = *i++;
			func(e);
		}

		cur = move(parent);
	}

	template<typename A, typename F>
	void array_obj(char const* name, A& a, F func)
	{
		arr(name, a, move(func));
	}

	void i32(char const* name, int& v)
	{
		value jv(f(name));
		if (jv.tt != t_integer) throw parse_error();
		v = jv.u.i;
	}

	void b(char const* name, bool& v)
	{
		value jv(f(name));
		if (jv.tt != t_bool) throw parse_error();
		v = jv.u.i != 0;
	}

	template<typename T, typename Resolver>
	void ref(char const* name, T& v, Resolver resolver)
	{
		value jv(f(name));
		if (jv.tt == t_null)
		{
			resolver.r2v(v);
		}
		else
		{
			if (jv.tt != t_string) throw parse_error();
			resolver.r2v(v, ((string*)jv.u.s)->s);
		}
	}

	void str(char const* name, std::string& s)
	{
		value jv(f(name));
		if (jv.tt != t_string) throw parse_error();
		s = ((string*)jv.u.s)->s;
	}

	//

	void next()
	{
		c = r.get_def();
	}

	void skipallws()
	{
		while (c == ' ' || c == '\r' || c == '\n' || c == '\t')
			next();
	}

	void check(uint8_t e)
	{
		if (c != e)
			throw parse_error();
		next();
	}

	bool test(uint8_t e)
	{
		if (c != e)
			return false;
		next();
		return true;
	}

	int from_hex(uint8_t c)
	{
		if (c >= 'a' && c <= 'f')
			return c - 'a' + 10;
		else if (c >= 'A' && c <= 'F')
			return c - 'A' + 10;
		else if (c >= '0' && c <= '9')
			return c - '0';
		return 0;
	}

	string val_str()
	{
		string s;

		check('"');
		while (c != '"')
		{
			if (c == '\\')
			{
				next();
				if (c == '\\' || c == '/' || c == '"')
				{
					s.s.push_back(c);
					next();
				}
				else if (c == 'u')
				{
					uint8_t c0 = r.get_def();
					uint8_t c1 = r.get_def();
					uint8_t c2 = r.get_def();
					uint8_t c3 = r.get_def();

					s.s.push_back((char)((from_hex(c2) << 4) + from_hex(c3)));
					next();
				}
				else
				{
					throw parse_error();
				}
			}
			else
			{
				s.s.push_back(c);
				next();
			}
		}
		check('"');

		return move(s);
	}

	value val()
	{
		if (c == '[')
		{
			array a;

			next();
			skipallws();
			
			while (c != ']')
			{
				a.v.emplace_back(val());
				skipallws();

				if (!c || c == ']')
					break;
				check(',');
				skipallws();
			}

			check(']');

			return value(move(a));
		}
		else if (c == 't')
		{
			next();
			check('r');
			check('u');
			check('e');
			return value(true);
		}
		else if (c == 'f')
		{
			next();
			check('a');
			check('l');
			check('s');
			check('e');
			return value(false);
		}
		else if (c == 'n')
		{
			next();
			check('u');
			check('l');
			check('l');
			return value();
		}
		else if (c == '"')
		{
			return value(val_str());
		}
		else if (c == '-' || (c >= '0' && c <= '9'))
		{
			bool neg = false;
			if (c == '-')
			{
				neg = true;
				next();
			}

			int v = 0;

			while (c >= '0' && c <= '9')
			{
				v = (v * 10) + (c - '0');
				next();
			}

			return value(neg ? -v : v);
		}

		throw parse_error();
	}

	Reader& r;
	uint8_t c;
};

}

}

#endif // GVL_SERIALIZATION_TOML_HPP
