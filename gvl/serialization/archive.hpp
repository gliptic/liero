#ifndef UUID_8FD050E2BE0F4345A60D1A8646927047
#define UUID_8FD050E2BE0F4345A60D1A8646927047

#include <stdexcept>
//#include <gvl/io/stream.hpp>
//#include <gvl/io/encoding.hpp>
#include "coding.hpp"
#include <gvl/serialization/context.hpp>
#include <gvl/support/cstdint.hpp>
#include <gvl/support/bits.hpp>
#include "except.hpp"

namespace gvl
{

template<typename Reader, typename Context = default_serialization_context>
struct in_archive
{
	static bool const in = true;
	static bool const out = false;
	static bool const reread = false;
	
	in_archive(Reader& reader, Context& context)
	: reader(reader), context(context)
	{
	}
	
	template<typename T>
	in_archive& i32(T& v)
	{
		v = uint32_as_int32(gvl::read_uint32(reader));
		return *this;
	}
	
	template<typename T>
	in_archive& i32_le(T& v)
	{
		v = uint32_as_int32(gvl::read_uint32_le(reader));
		return *this;
	}
	
	template<typename T>
	in_archive& ui16(T& v)
	{
		v = gvl::read_uint16(reader);
		return *this;
	}
	
	template<typename T>
	in_archive& ui16_le(T& v)
	{
		v = gvl::read_uint16_le(reader);
		return *this;
	}
	
	template<typename T>
	in_archive& ui32(T& v)
	{
		v = gvl::read_uint32(reader);
		return *this;
	}
	
	template<typename T>
	in_archive& ui32_le(T& v)
	{
		v = gvl::read_uint32_le(reader);
		return *this;
	}
	
	template<typename T>
	in_archive& ui8(T& v)
	{
		v = reader.get();
		return *this;
	}
	
	template<typename T>
	in_archive& b(T& v)
	{
		v = !!reader.get();
		return *this;
	}

	template<typename T>
	in_archive& str(T& v)
	{
		uint32_t len = gvl::read_uint32(reader);
		v.clear();
		for(uint32_t i = 0; i < len; ++i)
		{
			v.push_back((char)reader.get());
		}
		return *this;
	}
	
	template<typename T>
	in_archive& pascal_str(T& v, std::size_t field_len)
	{
		std::size_t len = reader.get();
		len = std::min(len, field_len - 1);
		std::size_t zeroes = field_len - 1 - len;
		
		v.clear();
		// TODO: Faster way
		for(std::size_t i = 0; i < len; ++i)
			v.push_back((char)reader.get());
		
		for(std::size_t i = 0; i < zeroes; ++i)
			reader.get(); // Ignore
		
		return *this;
	}
	
	template<typename T, typename Creator>
	in_archive& obj(T*& v, Creator creator)
	{
		uint32_t id = gvl::read_uint32(reader);
		if(context.read(v, id, creator))
			archive(*this, *v);
			
		return *this;
	}

	template<typename T>
	in_archive& obj(T*& v)
	{
		return obj(v, gvl::new_creator<T>());
	}

	template<typename T, typename Ref, typename Creator, typename RefCreator>
	in_archive& obj(Ref& v, Creator creator, RefCreator refCreator)
	{
		T* p;
		obj(p, creator);
		v = refCreator(p, context);
		return *this;
	}
	
	template<typename T, typename Creator>
	in_archive& obj(gvl::shared_ptr<T>& v, Creator creator)
	{
		T* p;
		obj(p, creator);
		v.reset(p);
		return *this;
	}
	
	template<typename T>
	in_archive& obj(gvl::shared_ptr<T>& v)
	{
		return obj(v, gvl::new_creator<T>());
	}
	
	template<typename T, typename Creator>
	in_archive& fobj(T*& v, Creator creator)
	{
		v = creator(context);
		archive(*this, *v);
			
		return *this;
	}
	
	template<typename T>
	in_archive& fobj(T*& v)
	{
		return fobj(v, gvl::new_creator<T>());
	}
	
	template<typename T, typename Creator>
	in_archive& fobj(gvl::shared_ptr<T>& v, Creator creator)
	{
		T* p;
		fobj(p, creator);
		v.reset(p);
		return *this;
	}
	
	template<typename T>
	in_archive& fobj(gvl::shared_ptr<T>& v)
	{
		return fobj(v, gvl::new_creator<T>());
	}
	
	in_archive& check()
	{
		uint32_t v = gvl::read_uint32(reader);
		if(v != 0x12345678)
			throw archive_check_error("Expected checkpoint here");
		return *this;
	}
	
	Reader& reader;
	Context& context;
};

template<
	typename Writer,
	typename Context = default_serialization_context,
	bool Reread = true>
struct out_archive
{
	static bool const in = false;
	static bool const out = true;
	static bool const reread = Reread;
	
	out_archive(Writer& writer, Context& context)
	: writer(writer), context(context)
	{
	}
	
	out_archive& i32(int32_t v)
	{
		gvl::write_uint32(writer, int32_as_uint32(v));
		return *this;
	}
	
	out_archive& i32_le(int32_t v)
	{
		gvl::write_uint32_le(writer, int32_as_uint32(v));
		return *this;
	}
	
	out_archive& ui16(uint32_t v)
	{
		
		gvl::write_uint16(writer, v);
		return *this;
	}
	
	out_archive& ui16_le(uint32_t v)
	{
		gvl::write_uint16_le(writer, v);
		return *this;
	}
	
	out_archive& ui32(uint32_t v)
	{
		gvl::write_uint32(writer, v);
		return *this;
	}
	
	out_archive& ui32_le(uint32_t v)
	{
		gvl::write_uint32_le(writer, v);
		return *this;
	}
	
	out_archive& ui8(uint32_t v)
	{
		sassert(v < 0x100);
		writer.put(v);
		return *this;
	}
	
	out_archive& b(bool v)
	{
		writer.put(v ? 1 : 0);
		return *this;
	}

	template<typename T>
	out_archive& str(T const& v)
	{
		gvl::write_uint32(writer, (uint32_t)v.size());
		for(uint32_t i = 0; i < v.size(); ++i)
		{
			writer.put((uint8_t)v[i]);
		}
		return *this;
	}
	
	template<typename T>
	out_archive& pascal_str(T const& v, std::size_t field_len)
	{
		std::size_t len = std::min(v.size(), field_len - 1);
		std::size_t zeroes = field_len - 1 - len;
		
		writer.put((uint8_t)len);
		writer.put(reinterpret_cast<uint8_t const*>(v.data()), len);
		for(std::size_t i = 0; i < zeroes; ++i)
			writer.put(0);
		
		return *this;
	}
	
	template<typename T, typename Creator>
	out_archive& obj(T*& v, Creator creator)
	{
		std::pair<bool, uint32_t> res = context.write(v);
		
		gvl::write_uint32(writer, res.second);
		if(res.first)
			archive(*this, *v);
		
		return *this;
	}

	template<typename T, typename Ref, typename Creator, typename RefCreator>
	out_archive& obj(Ref& v, Creator creator, RefCreator refCreator)
	{
		T* p = refCreator(v, context);
		return obj(p);
	}
	
	template<typename T>
	out_archive& obj(T*& v)
	{
		return obj(v, 0);
	}
	
	template<typename T, typename Creator>
	out_archive& obj(gvl::shared_ptr<T>& v, Creator creator)
	{
		T* p = v.get();
		return obj(p);
	}
	
	template<typename T>
	out_archive& obj(gvl::shared_ptr<T>& v)
	{
		return obj(v, 0);
	}
	
	template<typename T, typename Creator>
	out_archive& fobj(T*& v, Creator creator)
	{
		archive(*this, *v);
		
		return *this;
	}
	
	template<typename T>
	out_archive& fobj(T*& v)
	{
		return fobj(v, 0);
	}
	
	template<typename T, typename Creator>
	out_archive& fobj(gvl::shared_ptr<T>& v, Creator creator)
	{
		T* p = v.get();
		return fobj(p);
	}
	
	template<typename T>
	out_archive& fobj(gvl::shared_ptr<T>& v)
	{
		return fobj(v, 0);
	}
	
	out_archive& check()
	{
		gvl::write_uint32(writer, 0x12345678);
		return *this;
	}
	
	Writer& writer;
	Context& context;
};

template<typename Archive>
struct versioned_archive
{
	versioned_archive(Archive const& base, int version_at_least)
	: base(base), enable(base.context.version() >= version_at_least)
	{
	}
	
	versioned_archive(Archive const& base, bool enable)
	: base(base), enable(enable)
	{
	}
	
	#define FUNC(name) template<typename T> \
	versioned_archive& name(T& v, T const& def) { \
		if(enable) base.name(v); \
		else if(base.in || base.reread) v = def; \
		return *this; \
	}
	
	FUNC(ui32)
	FUNC(ui16)
	FUNC(ui8)
	FUNC(i32)
	FUNC(i16)
	FUNC(i8)
	FUNC(str)
	FUNC(b)
	
	#undef FUNC
	
	Archive base;
	bool enable;
};

template<typename Archive>
versioned_archive<Archive> enable_with_version(Archive const& ar, int version_at_least)
{
	return versioned_archive<Archive>(ar, version_at_least);
}

template<typename Archive>
versioned_archive<Archive> enable_when(Archive const& ar, bool enable)
{
	return versioned_archive<Archive>(ar, enable);
}

/*
template<typename Context, typename HashAccum>
struct hash_archive
{
	static bool const in = false;
	static bool const out = true;
	
	hash_archive(HashAccum& writer, Context& context)
	: writer(writer), context(context)
	{
	}
	
	hash_archive& i32(int32_t v)
	{
		writer.ui32(writer, int32_as_uint32(v));
		return *this;
	}
	
	hash_archive& ui16(uint32_t v)
	{
		writer.ui16(writer, v);
		return *this;
	}
	
	hash_archive& ui32(uint32_t v)
	{
		writer.ui32(writer, v);
		return *this;
	}
	
	hash_archive& ui8(uint32_t v)
	{
		writer.ui8(v);
		return *this;
	}
	
	hash_archive& b(bool v)
	{
		writer.ui8(v ? 1 : 0);
		return *this;
	}

	template<typename T>
	hash_archive& str(T const& v)
	{
		writer.ui32(v.size());
		for(uint32_t i = 0; i < v.size(); ++i)
		{
			writer.ui8((uint8_t)v[i]);
		}
		return *this;
	}
	
	template<typename T, typename Creator>
	hash_archive& obj(T*& v, Creator creator)
	{
		std::pair<bool, uint32_t> res = context.write(v);
		
		writer.ui32(res.second);
		if(res.first)
			archive(*this, *v);
		
		return *this;
	}
	
	template<typename T>
	hash_archive& obj(T*& v)
	{
		return obj(v, 0);
	}
	
	template<typename T, typename Creator>
	hash_archive& obj(gvl::shared_ptr<T>& v, Creator creator)
	{
		T* p = v.get();
		return obj(p);
	}
	
	template<typename T>
	hash_archive& obj(gvl::shared_ptr<T>& v)
	{
		return obj(v, 0);
	}
	
	template<typename T, typename Creator>
	hash_archive& fobj(T*& v, Creator creator)
	{
		archive(*this, *v);
		
		return *this;
	}
	
	template<typename T>
	hash_archive& fobj(T*& v)
	{
		return fobj(v, 0);
	}
	
	template<typename T, typename Creator>
	hash_archive& fobj(gvl::shared_ptr<T>& v, Creator creator)
	{
		T* p = v.get();
		return fobj(p);
	}
	
	template<typename T>
	hash_archive& fobj(gvl::shared_ptr<T>& v)
	{
		return fobj(v, 0);
	}
	
	hash_archive& check()
	{
		writer.ui32(0x12345678);
		return *this;
	}
	
	HashAccum& writer;
	Context& context;
};*/

} // namespace gvl

#endif // UUID_8FD050E2BE0F4345A60D1A8646927047
