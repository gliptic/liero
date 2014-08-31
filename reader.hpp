#ifndef LIERO_READER_HPP
#define LIERO_READER_HPP

#include <cstdio>
#include <cstdlib>
#include <string>
#include <stdexcept>
#include <vector>
#include <gvl/cstdint.hpp>
#include <gvl/io2/stream.hpp>
#include <gvl/serialization/coding.hpp>
#include <gvl/support/platform.hpp>

extern std::string configRoot;

struct ReaderFile : gvl::noncopyable
{
	ReaderFile()
	: data(0), pos(0), len(0)
	{
	}

	ReaderFile(ReaderFile&& other)
	: data(other.data), pos(other.pos), len(other.len)
	{
		other.data = 0;
	}

	~ReaderFile()
	{
		std::free(data);
	}

	uint8_t* data;
	size_t pos, len;

	void seekg(size_t newPos)
	{
		if (newPos > len)
			throw gvl::stream_read_error(gvl::source_result::eos, "EOF in seekg()");
		pos = newPos;
	}

	size_t tellg()
	{
		return pos;
	}

	void skip(size_t step)
	{
		seekg(pos + step);
	}

	uint8_t get()
	{
		if (pos >= len)
			throw gvl::stream_read_error(gvl::source_result::eos, "EOF in get()");
		return data[pos++];
	}

	void get(uint8_t* p, size_t l)
	{
		if (pos + l > len)
			throw gvl::stream_read_error(gvl::source_result::eos, "EOF in get()");
		std::memcpy(p, data + pos, l);
		pos += l;
	}
};

inline std::string readPascalString(ReaderFile& f)
{
	unsigned char length = f.get();

	char txt[256];
	f.get(reinterpret_cast<uint8_t*>(txt), length);
	return std::string(txt, length);
}

inline std::string readPascalString(ReaderFile& f, unsigned char fieldLen)
{
	char txt[256];
	f.get(reinterpret_cast<uint8_t*>(txt), fieldLen);

	unsigned char length = static_cast<unsigned char>(txt[0]);
	return std::string(txt + 1, length);
}

inline std::string readPascalStringAt(ReaderFile& f, size_t location)
{
	f.seekg(location);
	return readPascalString(f);
}

inline uint32_t readUint8(ReaderFile& f)
{
	return f.get();
}

inline int32_t readSint8(ReaderFile& f)
{
	return (int8_t)f.get();
}

inline uint32_t readUint16(ReaderFile& f)
{
	return gvl::read_uint16_le(f);
}

inline int32_t readSint16(ReaderFile& f)
{
	return (int)(int16_t)gvl::read_uint16_le(f);
}

inline uint32_t readUint32(ReaderFile& f)
{
	return gvl::read_uint32_le(f);
}

inline int32_t readSint32(ReaderFile& f)
{
	return (int32_t)gvl::read_uint32_le(f);
}

void setConfigPath(std::string const& path);

#endif // LIERO_READER_HPP
