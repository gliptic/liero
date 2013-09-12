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

#if 1
sink_result octet_writer::flush()
{
	auto res = flush_buffer(default_initial_bucket_size);
	if(res.s == sink_result::error)
		return res;
		
	if (sink_)
		return sink_->flush();
	// If flush_buffer succeeded, we're ok without a sink
	return sink_result(sink_result::ok);
}

sink_result octet_writer::flush_buffer(bucket_size new_buffer_size)
{
	std::size_t size = buffer_size_();

	sink_result res = sink_result(sink_result::ok);

	if (size > 0)
	{
		if(!sink_) return sink_result(sink_result::error);

		correct_buffer_();
		res = sink_->write(std::move(buffer_));

		if (res.s != sink_result::ok
		 && res.s != sink_result::would_block)
			return res;
	}

	new_buffer_size = std::max(new_buffer_size, default_initial_bucket_size);

	if (!buffer_)
	{
		buffer_.reset(bucket_data_mem::create(new_buffer_size, new_buffer_size));
		read_in_buffer_();
	}
	else
	{
		auto old_size = std::size_t(cur_ - end_);
		auto new_size = std::max(old_size + new_buffer_size, old_size * 2);
		buffer_.reset(buffer_->enlarge(new_size));
		buffer_->size_ = new_size;
		cur_ = buffer_->data + old_size;
		end_ = buffer_->data + buffer_->size_;
	}

	sassert(left() >= new_buffer_size);
	
	return res;
}

sink_result octet_writer::overflow_put_(uint8_t b)
{
	auto ret = flush_buffer(default_initial_bucket_size);
	sassert(cur_ != end_);
	*cur_++ = b;
	return ret;
}

sink_result octet_writer::overflow_put_(uint8_t const* p, std::size_t len)
{
	check_sink();

	sink_result ret(sink_result::ok);

	while (len > left())
	{
		std::size_t l = left();
		// Copy as much as we can
		std::memcpy(cur_, p, l);
		cur_ += l;
		p += l;
		len -= l;
			
		// Flush and try to allocate a buffer large enough for the rest of the data
		ret = flush_buffer(len);
		if (ret.s == sink_result::error)
			return ret;
	}

	std::memcpy(cur_, p, len);
	cur_ += len;
	return ret;
}
#endif

}
