#include "encoding.hpp"

namespace gvl
{

stream::read_result octet_stream_reader::get_bucket(size_type amount)
{
	if(first_.get())
	{
		correct_first_bucket_();
		return stream::read_result(stream::read_ok, first_.release());
	}
	else
		return read_bucket_and_return_(amount);
}
	
// Non-blocking
stream::read_result octet_stream_reader::try_get_bucket(size_type amount)
{
	if(first_.get())
	{
		correct_first_bucket_();
		return stream::read_result(stream::read_ok, first_.release());
	}
	else
		return try_read_bucket_and_return_(amount);
}
	
shared_ptr<stream> octet_stream_reader::detach()
{
	if(has_source())
	{
		correct_first_bucket_();

		shared_ptr<stream> ret = source_.release();
			
		if(first_.get())
			ret->unread(first_.release());
		cur_ = end_ = 0;
		sassert(cur_ == end_);
		sassert(!first_.get());

		return ret;
	}
	else
		return source_.release();
}

uint8_t octet_stream_reader::underflow_get_()
{
	stream::read_status status = next_bucket_();
	if(status != stream::read_ok)
		throw stream_read_error(status, "Read error in get()");
		
	return *cur_++;
}
	
stream::read_status octet_stream_reader::underflow_get_(uint8_t& ret)
{
	if(!source_)
		return stream::read_error;

	stream::read_status s = next_bucket_();
	if(s != stream::read_ok)
		return s;
		
	ret = *cur_++;
	return stream::read_ok;
}

stream::read_status octet_stream_reader::next_bucket_(uint32_t amount)
{
	passert(cur_ == end_, "Still data in the first bucket");
	check_source();
		
	// Need to read a bucket
		
	// Reset first
	// No need to do this: cur_ = end_ = 0;
	first_.reset();
		
	//while(true)
	{
		stream::read_result r(source_->read(amount));

		if(r.s == stream::read_ok)
		{
			// Callers of next_bucket_ expect the result
			// in first_
			set_first_bucket_(r.b);
			return stream::read_ok;
		}
		else if(r.s == stream::read_eos)
		{
			return stream::read_eos;
		}
			
		// TODO: derived()->block();
	}
		
	return stream::read_blocking;
}

stream::read_result octet_stream_reader::read_bucket_and_return_(size_type amount)
{
	check_source();
	//while(true)
	{
		stream::read_result r(source_->read(amount));
		
		if(r.s != stream::read_blocking)
			return r;
			
		/* TODO:
		derived()->flush();
		derived()->block();
		*/
	}
		
	return stream::read_result(stream::read_blocking);
}

stream::write_status octet_stream_writer::flush_buffer(bucket_size new_buffer_size)
{
	std::size_t size = buffer_size_();
	if(size > 0)
	{
		if(!sink_) return stream::write_error;

		sassert((cap_ & (cap_ - 1)) == 0);
			
		correct_buffer_();
		gvl::list<bucket> b;
		b.push_back(new bucket(buffer_.release(), 0, size));
		
		stream::write_result res = sink_->write_or_buffer(b.first());

		// TODO: Improve
		if(size > estimated_needed_buffer_size_)
			estimated_needed_buffer_size_ <<= 1;
		else if(size + 256 < estimated_needed_buffer_size_)
			estimated_needed_buffer_size_ >>= 1;
		estimated_needed_buffer_size_ = std::max(estimated_needed_buffer_size_, bucket_size(default_initial_bucket_size));
		
		if(new_buffer_size == 0)
			cap_ = estimated_needed_buffer_size_;
		else
		{
			cap_ = 1;
			while(cap_ < new_buffer_size)
				cap_ *= 2;
		}
		cap_ = std::min(cap_, max_bucket_size);

		buffer_.reset(bucket_data_mem::create(cap_, 0));
		
		cur_ = buffer_->data;
		end_ = buffer_->data + cap_;
		
		return res.s;
	}
	
	return stream::write_ok;
}

stream::write_status octet_stream_writer::weak_flush(bucket_size new_buffer_size)
{
	return flush_buffer(new_buffer_size);
}

stream::write_status octet_stream_writer::flush(bucket_size new_buffer_size)
{
	stream::write_status res = flush_buffer(new_buffer_size);
	if(res != stream::write_ok)
		return res;
		
	if(sink_)
		return sink_->flush();
	// If flush_buffer succeeded, we're ok without a sink
	return stream::write_ok;
}

stream::write_status octet_stream_writer::put_bucket(bucket* buf)
{
	stream::write_status res = flush_buffer();

	stream::write_result res2 = sink_->write_or_buffer(buf);
	return res != stream::write_ok ? res : res2.s;
}

stream::write_status octet_stream_writer::overflow_put_(uint8_t b)
{
	check_sink();
		
	if(buffer_size_() >= max_bucket_size)
	{
		stream::write_status ret = weak_flush();
		sassert(cur_ != end_);
		*cur_++ = b;
		return ret;
	}
	else
	{
		correct_buffer_();
		cap_ *= 2;
		buffer_.reset(buffer_->enlarge(cap_));
		buffer_->unsafe_push_back(b);
			
		read_in_buffer_();
		return stream::write_ok;
	}
}
	
	
stream::write_status octet_stream_writer::overflow_put_(uint8_t const* p, std::size_t len)
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
	
void octet_stream_writer::ensure_cap_(std::size_t s)
{
	if(cap_ < s)
	{
		correct_buffer_();
		while(cap_ < s)
			cap_ *= 2;
		buffer_.reset(buffer_->enlarge(cap_));
		//sassert(size_ == buffer_->size_);
		cur_ = buffer_->data + buffer_->size_;
		end_ = buffer_->data + cap_;
		sassert(std::size_t(cur_ - buffer_->data) == buffer_->size_);
	}
}

}
