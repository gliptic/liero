#include "stream.hpp"

namespace gvl
{
	
shared_ptr<stream_piece> octet_reader::detach()
{
	if (cur_ == end_)
		return head_;

	shared_ptr<stream_piece> ret(new stream_piece(head_));

	ret->data.reset(bucket_data_mem::create_from(cur_, end_, end_ - cur_));

	close();
	return ret;
}

void octet_reader::close()
{
	cur_ = 0;
	end_ = 0;
	cur_data.reset();
	head_.reset();
}

uint8_t octet_reader::underflow_get_()
{
	auto status = next_piece_();
	if (status != source_result::ok)
		throw runtime_error("Read error in get()");
		
	return *cur_++;
}
	
source_result::status octet_reader::underflow_get_(uint8_t& ret)
{
	if(!head_)
		return source_result::eos;

	auto s = next_piece_();
	if (s != source_result::ok)
		return s;
		
	ret = *cur_++;
	return source_result::ok;
}

void octet_reader::set_bucket_(shared_ptr<bucket_data_mem> bucket)
{
	cur_data = bucket;
	cur_ = cur_data->begin();
	end_ = cur_data->end();
}

source_result::status octet_reader::next_piece_(uint32_t amount)
{
	passert(cur_ == end_, "Still data in the first bucket");

	if (!head_)
		return source_result::eos;

	auto s = head_->ensure_data();
	if (s != source_result::ok)
	{
		if (s == source_result::eos)
			close();
		return s;
	}

	set_bucket_(head_->data);

	head_ = head_->next;
	return source_result::ok;
}

//

#if 0
sink_result octet_writer::flush(bucket_size new_buffer_size)
{
	auto res = flush_buffer(new_buffer_size);
	if(res.s != sink_result::ok)
		return res;
		
	if(sink_)
		return sink_->flush();
	// If flush_buffer succeeded, we're ok without a sink
	return sink_result(sink_result::ok);
}

sink_result octet_writer::overflow_put_(uint8_t b)
{
	auto ret = weak_flush();
	sassert(cur_ != end_);
	*cur_++ = b;
	return ret;
}
	
	
sink_result octet_writer::overflow_put_(uint8_t const* p, std::size_t len)
{
	check_sink();
		
	// As long as fitting in the current buffer would make it
	// too large, write as much as possible and flush.
	while((cur_ - buffer_->data) + len >= max_bucket_size)
	{
		std::size_t left = end_ - cur_;
		// Copy as much as we can
		std::memcpy(cur_, p, left);
		cur_ += left;
		p += left;
		len -= left;
			
		// Flush and try to allocate a buffer large enough for the rest of the data
		stream::write_status ret = weak_flush(len);
		if(ret != stream::write_ok)
			return ret;
	}
		
	// Write the rest
	ensure_cap_((cur_ - buffer_->data) + len);
		
	std::memcpy(cur_, p, len);
	cur_ += len;
	return stream::write_ok;
}
#endif

}
