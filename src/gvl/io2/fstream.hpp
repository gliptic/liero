#ifndef GVL_IO2_FSTREAM_HPP
#define GVL_IO2_FSTREAM_HPP

#include "stream.hpp"

#include <cstdio>
#include <stdexcept>

namespace gvl
{

struct file_bucket_pipe : bucket_pipe
{
	file_bucket_pipe()
	: f(0)
	{
	}

	file_bucket_pipe(char const* path, char const* mode)
	{
		FILE* f_init = std::fopen(path, mode);
		init(f_init);
	}

	file_bucket_pipe(FILE* f_init)
	{
		init(f_init);
	}

	void init(FILE* f_init)
	{
		f = f_init;
		if(!f)
			throw std::runtime_error("Couldn't open file");
	}

	void close()
	{
		if(f)
		{
			std::fclose(f);
			f = 0;
		}
	}

	~file_bucket_pipe()
	{
		close();
	}

	virtual source_result read_next(size_t amount = 0)
	{
		if (!f)
			return std::unique_ptr<bucket_data_mem>();

		std::size_t file_buf_size = 1 << 16;

		unique_ptr<bucket_data_mem> r(bucket_data_mem::create(file_buf_size));

		auto read_bytes = std::fread(r->begin(), 1, file_buf_size, f);
		if(read_bytes == 0)
			return source_result(source_result::eos);

		r->size_ = read_bytes;
		return source_result(std::move(r));
	}

	virtual sink_result write(unique_ptr<bucket_data_mem>&& data)
	{
		auto write_bytes = std::fwrite(data->begin(), data->size(), 1, f);

		if (write_bytes == 0)
			return sink_result(sink_result::error);
		data.reset();
		return sink_result(sink_result::ok);
	}

	virtual sink_result flush()
	{
		// Everything is flushed
		return sink_result(sink_result::ok);
	}

	FILE* f;
};

}


#endif // GVL_IO2_FSTREAM_HPP
