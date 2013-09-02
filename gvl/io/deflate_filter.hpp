#ifndef UUID_1EB2DB01C1DC4C74569FE0B1E099291B
#define UUID_1EB2DB01C1DC4C74569FE0B1E099291B

#include "stream.hpp"
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "../zlib/zlib2.h"

namespace gvl
{

// TODO: IMPORTANT: apply should return read_ok if it appends a bucket to in_buffer,
// otherwise it should return something else. Make sure this happens everywhere.

struct deflate_filter : filter
{
	deflate_filter(bool compress)
	: compress(compress)
	, ended(false)
	{
		str.zalloc = 0;
		str.zfree = 0;
		str.opaque = 0;
		
		if(compress)
			mz_deflateInit(&str, MZ_DEFAULT_COMPRESSION);
		else
			mz_inflateInit(&str);
		
		str.avail_in = 0;
		
		prepare_out_bucket();
	}
	
	~deflate_filter()
	{
		close();
		
		if(compress)
			mz_deflateEnd(&str);
		else
			mz_inflateEnd(&str);
	}
	
	read_status apply(apply_mode mode, size_type amount = 0)
	{
		if(ended)
			return read_blocking;
			
		bool written = false;
		
		int deflate_flags = MZ_NO_FLUSH;
		if(mode == am_flushing)
			deflate_flags = MZ_SYNC_FLUSH;
		else if(mode == am_closing)
		{
			if(compress)
				deflate_flags = MZ_FINISH;
			else
				deflate_flags = MZ_SYNC_FLUSH;
		}

		while(true)
		{
			if(str.avail_in == 0)
			{
				if(filter_buffer.empty())
				{
					if(mode == am_non_pulling)
					{
						break; // Done
					}
					else if(mode == am_pulling)
					{
						read_result res = source->read(amount);
						if(res.s != read_ok)
						{
							if(str.next_out != &buffer->data[0])
							{
								// We have a bucket, fine
								break;
							}
							else
							{
								return res.s;
							}
						}
						else
						{
							prepare_in_bucket(res.b);
						}
					}
				}
				else
				{
					prepare_in_bucket(filter_buffer.unlink_first());
				}
			}

			if(str.avail_out == 0)
			{
				written = true;
				prepare_out_bucket();
				if(mode == am_pulling)
					break;
			}
			
			/*
				am_non_pulling:
					Just run deflate until there is no more buffered input.
				am_pulling:
					Run deflate until we cannot pull anything more OR
					we have written one bucket.
				am_flushing:
					Run deflate until there is no more buffered input
				am_closing
					Run deflate until Z_STREAM_END is returned
			*/
				
			int ret;
			if(compress)
				ret = mz_deflate(&str, deflate_flags);
			else
				ret = mz_inflate(&str, deflate_flags);
			
			if(ret == MZ_STREAM_END)
			{
				ended = true;
				break;
			}
			else if(ret != MZ_OK)
			{
				printf("avail_in = %d, avail_out = %d\n", str.avail_in, str.avail_out);
				throw stream_error("Error while deflating");
			}

			if((deflate_flags == MZ_SYNC_FLUSH) && str.avail_in == 0)
				break;
		}
		
		// Force away any out_bucket when flushing or closing
		if((mode == am_flushing || mode == am_closing || mode == am_pulling)
		&&  str.next_out != &buffer->data[0])
		{
			// We have a bucket, fine, pass it through and make a new.
			written = true;
			prepare_out_bucket();
		}
		
		return written ? read_ok : read_blocking;
	}
		
	void prepare_in_bucket(bucket* in_bucket_new)
	{
		in_bucket.reset(in_bucket_new);
		str.next_in = reinterpret_cast<const unsigned char*>(const_cast<uint8_t*>(in_bucket->get_ptr()));
		str.avail_in = (unsigned int)(in_bucket->size());
	}
	
	void prepare_out_bucket()
	{
		if(buffer.get())
		{
			buffer->size_ = str.next_out - &buffer->data[0];
			in_buffer.append(new gvl::bucket(buffer.release()));
		}
		
		std::size_t const buf_size = 1024;
		buffer.reset(gvl::bucket_data_mem::create(buf_size, 0));
		str.avail_out = buf_size;
		str.next_out = reinterpret_cast<unsigned char*>(&buffer->data[0]);
	}
	
	mz_stream str;
	std::auto_ptr<bucket_data_mem> buffer;
	std::auto_ptr<bucket> in_bucket;
	
	bool compress;
	bool ended; // TODO: The stream should have some state for closed
};

}

#endif // UUID_1EB2DB01C1DC4C74569FE0B1E099291B
