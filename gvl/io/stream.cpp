#include "stream.hpp"

//#include "../support/log.hpp"
#include <utility>

namespace gvl
{

bucket::bucket(void const* ptr, size_type len)
{
	bucket_data_mem* data_init = bucket_data_mem::create(len, len);
	std::memcpy(data_init->data, ptr, len);
	data_.reset(data_init);
	begin_ = 0;
	end_ = len;
}


#if 0
stream::write_status octet_stream_writer::partial_flush()
{
	if(!sink_)
		throw stream_write_error(stream::write_error, "No sink assigned to octet_stream_writer");
	stream::write_status stat = stream::write_ok;
	while(!mem_buckets_.empty())
	{
		stream::write_result res = sink_->write(mem_buckets_.first());
		stat = res.s;
		if(!res.consumed)
			break;
	}
	
	// Buffered remaining
	if(!mem_buckets_.empty())
	{
		sink_->write_buffered(mem_buckets_);
	}
	
	return stat;
}
#endif


}
