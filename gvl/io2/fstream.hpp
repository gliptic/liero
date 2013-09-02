#ifndef GVL_IO2_FSTREAM_HPP
#define GVL_IO2_FSTREAM_HPP

#include "stream.hpp"

#include <cstdio>
#include <stdexcept>

namespace gvl
{

struct file_bucket_source : bucket_pipe
{
	file_bucket_source()
	: f(0)
	{
	}

	file_bucket_source(char const* path, char const* mode)
	{
		FILE* f_init = std::fopen(path, mode);
		init(f_init);
	}
	
	file_bucket_source(FILE* f_init)
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
	
	~file_bucket_source()
	{
		close();
	}

	virtual source_result read_next(size_t amount = 0)
	{
		if (!f)
			return std::unique_ptr<bucket_data_mem>();

		unique_ptr<bucket_data_mem> r(bucket_data_mem::create(4096));

		auto read_bytes = std::fread(r->begin(), 1, 4096, f);
		if(read_bytes == 0)
			return source_result(source_result::eos);

		r->size_ = read_bytes;
		return source_result(std::move(r));
	}

	virtual sink_result write(unique_ptr<bucket_data_mem>&& data)
	{
		auto write_bytes = std::fwrite(data->begin(), 1, data->size(), f);

		if (write_bytes == 0)
			return sink_result(sink_result::error);
		if (write_bytes != data->size())
			return sink_result(sink_result::part);
		return sink_result(sink_result::ok);
	}
	
	FILE* f;
};

}


#endif // GVL_IO2_FSTREAM_HPP
