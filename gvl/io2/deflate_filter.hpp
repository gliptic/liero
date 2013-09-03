#ifndef GVL_IO2_DEFLATE_FILTER_HPP
#define GVL_IO2_DEFLATE_FILTER_HPP

#include "stream.hpp"
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "../zlib/zlib2.h"
#include <memory>

namespace gvl
{

struct deflate_source : bucket_pipe, octet_reader
{
	mz_stream str;
	bool pull, compress;

	shared_ptr<bucket_pipe> sink;
	std::unique_ptr<bucket_data_mem> cur_out;

	deflate_source(source const& src, bool compress)
	: octet_reader(src)
	, pull(true)
	, compress(compress)
	{
		str.zalloc = 0;
		str.zfree = 0;
		str.opaque = 0;
		
		if (compress)
			mz_deflateInit(&str, MZ_DEFAULT_COMPRESSION);
		else
			mz_inflateInit(&str);
		
		str.avail_in = 0;
		str.avail_out = 0;

		prepare_out();
	}

	~deflate_source()
	{
		mz_inflateEnd(&str);
	}

	void prepare_out()
	{
		cur_out.reset(bucket_data_mem::create(65536, 0));
		str.next_out = (unsigned char*)cur_out->begin();
		str.avail_out = 65536;
	}

	virtual source_result read_next(size_t amount = 0)
	{
		sassert(pull);

		auto s = drive();
		if (s == source_result::ok)
		{
			auto bucket = std::move(cur_out);
			prepare_out();
			return source_result(std::move(bucket));
		}

		return source_result(s);
	}

	/*

	* If ready buffered output, try to push OR quit
	* If still input to process, try to drive and push OR quit
	* Fill input, try to drive

	*/

#if 1
	sink_result try_write_cur()
	{
		auto r = sink->write(std::move(cur_out));
		if (!r)
		{
			if (r.s == sink_result::part)
				return sink_result(sink_result::would_block);
			return r;
		}

		prepare_out();
		return r;
	}

	virtual sink_result write(unique_ptr<bucket_data_mem>&& data)
	{
		sassert(!pull);

		while (true)
		{
			if (str.avail_out == 0)
			{
				// TODO: cur_out has not been written, try that

				auto r = try_write_cur();
				if (!r) return r;

				sassert(str.avail_out != 0);
			}
			else if (str.avail_in != 0)
			{
				sassert(str.avail_out != 0);

				auto s = drive();
				if (s != source_result::ok)
					return sink_result::would_block; // TODO: Should return error if the stream does

				sassert(str.avail_out == 0 || str.avail_in == 0);
				continue;
			}
			else
			{
				set_bucket_(shared_ptr<bucket_data_mem>(data.release()));

				auto s = drive();
				if (s != source_result::ok)
					return sink_result::would_block; // TODO: Should return error if the stream does

				try_write_cur();
				return sink_result::ok;
			}
		}
	}
#endif

	source_result::status drive(bool flush = false)
	{
		if (empty())
			return source_result::eos;
			
		int deflate_flags = flush ? MZ_SYNC_FLUSH : MZ_NO_FLUSH;

		while (true)
		{
			if (str.avail_in == 0 && pull)
			{
				cur_ = end_;
				auto s = next_piece_();
				if (s == source_result::ok)
				{
					str.avail_in = buf_left();
					sassert(str.avail_in > 0);
					str.next_in = (unsigned char*)cur_;
				}
				else if (s != source_result::eos)
				{
					return s;
				}
			}

			if (str.avail_out == 0)
			{
				cur_out->size_ = str.next_out - cur_out->data;
				
				return source_result::ok;
			}

			int ret;
			
			if (compress)
				ret = mz_deflate(&str, deflate_flags);
			else
				ret = mz_inflate(&str, deflate_flags);

			if (ret == MZ_STREAM_END)
			{
				cur_out->size_ = str.next_out - cur_out->data;

				close();
				return source_result::ok;
			}
			else if (ret != MZ_OK)
			{
				throw std::runtime_error("Error while deflating");
			}
			else if (deflate_flags == MZ_SYNC_FLUSH && str.avail_in == 0)
			{
				cur_out->size_ = str.next_out - cur_out->data;
				
				return source_result::ok;
			}
		}
	}
};

}

#endif // GVL_IO2_DEFLATE_FILTER_HPP
