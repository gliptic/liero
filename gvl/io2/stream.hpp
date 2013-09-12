#ifndef GVL_IO2_STREAM_HPP
#define GVL_IO2_STREAM_HPP

#include "../resman/shared_ptr.hpp"
#include "../containers/bucket.hpp"
#include "../support/debug.hpp"
#include "../io/convert.hpp"
#include <memory>

namespace gvl
{

using std::size_t;
using std::runtime_error;
using std::unique_ptr;

struct source_result
{
	enum status
	{
		ok = 0,
		blocking,
		eos,
		error,
	};

	source_result(source_result&& other)
	: s(other.s)
	, data(std::move(other.data))
	{
	}

	source_result(status s)
	: s(s)
	{
		
	}

	source_result(unique_ptr<bucket_data_mem> data)
	: s(ok)
	, data(std::move(data))
	{
		
	}

	status s;
	unique_ptr<bucket_data_mem> data;
};

struct sink_result
{
	enum status
	{
		ok = 0,
		part,
		would_block,
		error
	};

	sink_result(status s)
	: s(s)
	{
	}

	operator bool()
	{
		return s == ok;
	}

	status s;
};

struct bucket_pipe : shared
{
	virtual source_result read_next(size_t amount = 0)
	{
		return source_result(source_result::eos);
	}

	virtual sink_result write(unique_ptr<bucket_data_mem>&& data)
	{
		return sink_result(sink_result::would_block);
	}

	virtual sink_result flush()
	{
		return sink_result(sink_result::would_block);
	}
};

struct stream_piece : shared
{
	shared_ptr<bucket_data_mem> data;

	stream_piece(shared_ptr<stream_piece> const& next)
	: next(next)
	{
		
	}

	stream_piece(shared_ptr<bucket_pipe> const& source)
	: source(source)
	{
		
	}

	source_result::status ensure_data(size_t amount = 0)
	{
		if (data)
			return source_result::ok;

		// No bucket data, must read
		sassert(!next);

		auto r(source->read_next(amount));

		if (r.s != source_result::ok)
			return r.s;

		data.reset(r.data.release());

		next.reset(new stream_piece(source));
		source.reset(); // Don't need this any longer
		return source_result::ok;
	}

	shared_ptr<stream_piece> next;
	shared_ptr<bucket_pipe> source;
};

typedef shared_ptr<stream_piece> source;
typedef shared_ptr<bucket_pipe> sink;

inline source to_source(bucket_pipe* src)
{
	return source(new stream_piece(shared_ptr<bucket_pipe>(src)));
}

struct octet_reader
{
	typedef bucket::size_type size_type;
	
	octet_reader(source head)
	: cur_(0)
	, end_(0)
	, head_(head)
	{
	}
	
	octet_reader()
	: cur_(0)
	, end_(0)
	, head_()
	{
	}

	uint8_t get()
	{
		// We keep this function small to encourage inlining
		return (cur_ != end_) ? (*cur_++) : underflow_get_();
	}

	uint8_t get_def(uint8_t def = 0)
	{
		// We keep this function small to encourage inlining
		if (cur_ != end_)
		{
			return *cur_++;
		}
		else
		{
			underflow_get_(def);
			return def;
		}
	}
	
	void get(uint8_t* dest, std::size_t len)
	{
		while(true)
		{
			std::size_t piece = std::min(std::size_t(end_ - cur_), len);

			memcpy(dest, cur_, piece);
			dest += piece;
			cur_ += piece;
			len -= piece;
			if (len == 0)
				break;

			auto status = next_piece_(uint32_t(len));
			if (status != source_result::ok)
				throw runtime_error("Read error in get()");
		}
	}

	source_result::status try_skip()
	{
		if(cur_ != end_)
		{
			++cur_;
			return source_result::ok;
		}
		else
		{
			uint8_t dummy;
			return underflow_get_(dummy);
		}

	}
	
	bool try_get(uint8_t& ret)
	{
		if(cur_ != end_)
		{
			ret = *cur_++;
			return true;
		}
		else
		{
			return underflow_get_(ret) == source_result::ok;
		}
	}
	
	std::size_t try_get(uint8_t* dest, std::size_t len)
	{
		// TODO: Can optimize this
		for(std::size_t i = 0; i < len; ++i)
		{
			auto s = try_get(dest[i]);
			if(s != source_result::ok) return i;
		}

		return len;
	}

	std::size_t try_skip(std::size_t len)
	{
		// TODO: Can optimize this
		for(std::size_t i = 0; i < len; ++i)
		{
			auto s = try_skip();
			if(s != source_result::ok) return i;
		}

		return len;
	}

#if 0
	bool at_eos()
	{
		if(cur_ != end_)
			return false;
			
		auto status = next_piece_();
		if(status != source_result::ok)
			return true;
	
		return false; // TODO: Does erroring count as eos?	
	}
#endif
	
	shared_ptr<stream_piece> detach();

	void close();

	void attach(shared_ptr<stream_piece> head_new)
	{
		if(head_)
			throw runtime_error("A head is already attached to this octet_reader");
		
		head_ = head_new;
	}

	/// Amount of data left in buffer
	std::size_t buf_left() const { return end_ - cur_; }

	bool empty()
	{
		return cur_ == end_ && !head_;
	}
	
protected:
	
	uint8_t underflow_get_();
	source_result::status underflow_get_(uint8_t& ret);
	
	/// May throw.
	/// Precondition: cur_ == end_
	source_result::status next_piece_(uint32_t amount = 0);
	void set_bucket_(shared_ptr<bucket_data_mem> bucket);
	
	void check_head()
	{
		if(!head_)
			throw runtime_error("No head assigned to octet_stream_reader");
	}

	uint8_t const* cur_; // Pointer into head_->data
	uint8_t const* end_; // End of data in head_->data
	shared_ptr<bucket_data_mem> cur_data;
	shared_ptr<stream_piece> head_;
};

//

#if 1

struct octet_writer : basic_text_writer<octet_writer>
{
	
	static std::size_t const default_initial_bucket_size = 4096;
	
	octet_writer(shared_ptr<bucket_pipe> sink)
	: sink_()
	, cur_(0)
	, end_(0)
	{
		attach(sink);
	}
	
	octet_writer()
	: sink_()
	, cur_(0)
	, end_(0)
	{
	}
	
	~octet_writer()
	{
		if (sink_)
			flush();
	}
	
	sink_result flush();

	sink_result put(uint8_t b)
	{
		// We keep this function small to encourage
		// inlining of the common case
		return (cur_ != end_) ? (*cur_++ = b, sink_result(sink_result::ok)) : overflow_put_(b);
	}
	
	sink_result put(uint8_t const* p, std::size_t len)
	{
		// We keep this function small to encourage
		// inlining of the common case
		if(left() >= len)
		{
#if GVL_X86 || GVL_X86_64 // TODO: A define that says whether unaligned access is allowed
			if(len < 64) // TODO: Tweak this limit
			{
				while(len > 4)
				{
					*reinterpret_cast<uint32_t*>(cur_) = *reinterpret_cast<uint32_t const*>(p);
					len -= 4;
					cur_ += 4;
					p += 4;
				}
				while(len--)
					*cur_++ = *p++;
					
				return sink_result(sink_result::ok);
			}
#endif
			std::memcpy(cur_, p, len);
			cur_ += len;
			return sink_result(sink_result::ok);
		}
		else
		{
			return overflow_put_(p, len);
		}
	}
	
	shared_ptr<bucket_pipe> detach()
	{
		if (has_sink())
		{
			flush_buffer();
		
			// Buffer any remaining buckets
			// partial_flush already does this: sink_->write_buffered(mem_buckets_);
		}
		
		return sink_.release();
	}

	bucket_pipe& sink()
	{ return *sink_; }

	bool has_sink() const
	{ return sink_.get() != 0; }
	
	void attach(shared_ptr<bucket_pipe> new_sink)
	{
		if (sink_)
			throw runtime_error("A sink is already attached to the octet_writer");
		sink_ = new_sink;
		buffer_.reset(bucket_data_mem::create(default_initial_bucket_size, default_initial_bucket_size));
		read_in_buffer_();
	}
	
	void check_sink()
	{
		if(!sink_)
			throw runtime_error("No sink assigned to octet_writer");
	}
	
	void swap(octet_writer& b)
	{
		sink_.swap(b.sink_);
		std::swap(cur_, b.cur_);
		std::swap(end_, b.end_);
		
		buffer_.swap(b.buffer_);
	}
	
	/*
	void reserve(std::size_t size)
	{
		ensure_cap_(size);
	}
	
	// Make the growth of the current bucket unlimited
	void set_unlimited_bucket()
	{
		max_bucket_size = std::size_t(0) - 1;
	}*/
		
private:
	sink_result flush_buffer(bucket_size new_buffer_size = 0);
	
	std::size_t buffer_size_()
	{
		return (cur_ - buffer_->data);
	}

	std::size_t left() const
	{
		return end_ - cur_;
	}
	
	void correct_buffer_()
	{
		buffer_->size_ = buffer_size_();
	}
	
	void read_in_buffer_()
	{
		cur_ = buffer_->data;
		end_ = buffer_->data + buffer_->size_;
	}
	
	sink_result overflow_put_(uint8_t b);
	sink_result overflow_put_(uint8_t const* p, std::size_t len);
	
	shared_ptr<bucket_pipe> sink_;
	uint8_t* cur_; // Pointer into buffer_
	uint8_t* end_; // End of capacity in buffer_
	std::unique_ptr<bucket_data_mem> buffer_;
};

#endif

}

#endif // GVL_IO2_STREAM_HPP
