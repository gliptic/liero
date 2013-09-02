#ifndef GVL_SERIALIZATION_CODING_HPP
#define GVL_SERIALIZATION_CODING_HPP

#include <string>
#include <stdexcept>
#include "../support/cstdint.hpp"

namespace gvl
{

template<typename Writer>
inline void write_uint16(Writer& writer, unsigned int v)
{
	sassert(v < 0x10000);
	writer.put(uint8_t((v >> 8) & 0xff));
	writer.put(uint8_t(v & 0xff));
}

template<typename Writer>
inline void write_sint16(Writer& writer, unsigned int v)
{
	write_uint16(writer, v + 0x8000);
}

template<typename Writer>
inline void write_uint16_le(Writer& writer, unsigned int v)
{
	sassert(v < 0x10000);
	writer.put(uint8_t(v & 0xff));
	writer.put(uint8_t((v >> 8) & 0xff));
}

template<typename Writer>
inline void write_uint24(Writer& writer, unsigned int v)
{
	sassert(v < 0x1000000);
	writer.put(uint8_t((v >> 16) & 0xff));
	writer.put(uint8_t((v >> 8) & 0xff));
	writer.put(uint8_t(v & 0xff));
}

template<typename Writer>
inline void write_uint24_le(Writer& writer, unsigned int v)
{
	sassert(v < 0x1000000);
	writer.put(uint8_t(v & 0xff));
	writer.put(uint8_t((v >> 8) & 0xff));
	writer.put(uint8_t((v >> 16) & 0xff));
}

template<typename Writer>
inline void write_uint32(Writer& writer, uint32_t v)
{
	writer.put(uint8_t((v >> 24) & 0xff));
	writer.put(uint8_t((v >> 16) & 0xff));
	writer.put(uint8_t((v >> 8) & 0xff));
	writer.put(uint8_t(v & 0xff));
}

template<typename Writer>
inline void write_uint32_le(Writer& writer, uint32_t v)
{
	writer.put(uint8_t(v & 0xff));
	writer.put(uint8_t((v >> 8) & 0xff));
	writer.put(uint8_t((v >> 16) & 0xff));
	writer.put(uint8_t((v >> 24) & 0xff));
}

template<typename Writer>
inline void write_sint32(Writer& writer, int v)
{
	write_uint32(writer, v + 0x80000000);
}

template<typename Writer>
void write_string16(Writer& writer, std::string const& src)
{
	int len = src.size();
	if(len > 65535)
		len = 65535;
	write_uint16(writer, len);
	writer.put(reinterpret_cast<uint8_t const*>(src.data()), len);
}

template<typename Writer>
void write_aint(Writer& writer, unsigned int v)
{
	while(true)
	{
		int b = v & 0x7f;
		v >>= 7;
			
		if(v != 0)
			writer.put(b | 0x80);
		else
		{
			writer.put(b);
			break;
		}
	}
}

template<typename Reader>
unsigned int read_aint(Reader& reader)
{
	v = 0;

	for(int i = 0; i < 5; ++i)
	{
		uint8_t b = reader.get();

		if((b & 0x80) == 0)
		{
			v |= b;
			return v; // Ok
		}
		else
		{
			v = (v << 7) | (b & 0x7f);
		}
	}
		
	throw std::runtime_error("Malformed aint in read_aint");
}

template<typename Reader>
inline unsigned int read_uint24(Reader& reader)
{
	unsigned int ret = reader.get() << 16;
	ret |= reader.get() << 8;
	ret |= reader.get();
	return ret;
}

template<typename Reader>
inline unsigned int read_uint24_le(Reader& reader)
{
	unsigned int ret = reader.get();
	ret |= reader.get() << 8;
	ret |= reader.get() << 16;
	return ret;
}

template<typename Reader>
inline unsigned int read_uint16(Reader& reader)
{
	unsigned int ret = reader.get() << 8;
	ret |= reader.get();
	return ret;
}

template<typename Reader>
inline unsigned int read_uint16_le(Reader& reader)
{
	unsigned int ret = reader.get();
	ret |= reader.get() << 8;
	return ret;
}

template<typename Reader>
inline uint32_t read_uint32(Reader& reader)
{
	unsigned int ret = reader.get() << 24;
	ret |= reader.get() << 16;
	ret |= reader.get() << 8;
	ret |= reader.get();
	return ret;
}

template<typename Reader>
inline uint32_t read_uint32_le(Reader& reader)
{
	unsigned int ret = reader.get();
	ret |= reader.get() << 8;
	ret |= reader.get() << 16;
	ret |= reader.get() << 24;
	return ret;
}

template<typename Reader>
inline uint64_t read_uint64_le(Reader& reader)
{
	uint64_t ret = reader.get();
	ret |= uint64_t(reader.get()) << 8;
	ret |= uint64_t(reader.get()) << 16;
	ret |= uint64_t(reader.get()) << 24;
	ret |= uint64_t(reader.get()) << 32;
	ret |= uint64_t(reader.get()) << 40;
	ret |= uint64_t(reader.get()) << 48;
	ret |= uint64_t(reader.get()) << 56;
	return ret;
}



template<typename Reader>
inline int read_sint16(Reader& reader)
{
	return read_uint16(reader) - 0x8000;
}

template<typename Reader>
inline int read_sint32(Reader& reader)
{
	return read_uint32(reader) - 0x80000000;
}

template<typename Reader>
void read_string16(Reader& reader, std::string& dest)
{
	unsigned int len = read_uint16(reader);
	std::vector<char> buf(len + 1); // + 1 to avoid zero-length buf that would make buf[0] undefined
	reader.get(reinterpret_cast<uint8_t*>(&buf[0]), len);

	dest.assign(&buf[0], len);
}

}

#endif // GVL_SERIALIZATION_CODING_HPP
