#ifndef LIERO_READER_HPP
#define LIERO_READER_HPP

#include <cstdio>
#include <cstdlib>
#include <string>
#include <stdexcept>
#include <gvl/cstdint.hpp>
#include <gvl/io2/stream.hpp>
#include <gvl/serialization/coding.hpp>
#include <gvl/support/platform.hpp>

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

	explicit ReaderFile(gvl::source source)
	: data(0), pos(0)
	{
		len = 0;
		auto cur = source;
		while (cur && cur->ensure_data() == gvl::source_result::ok)
		{
			len += cur->data->size();
			cur = cur->next;
		}

		data = (uint8_t*)malloc(len);
		uint8_t* p = data;

		cur = source;
		while (cur && cur->data)
		{
			std::memcpy(p, cur->data->begin(), cur->data->size());
			p += cur->data->size();
			cur = cur->next;
		}
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

#endif // LIERO_READER_HPP
