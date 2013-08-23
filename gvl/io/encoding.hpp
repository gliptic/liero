#ifndef UUID_ADEA58A2C81F42C25E8CAFA32ED72A18
#define UUID_ADEA58A2C81F42C25E8CAFA32ED72A18

#include "stream.hpp"
#include "convert.hpp"
#include "../resman/shared.hpp"
//#include "../support/algorithm.hpp"
#include "../support/platform.hpp"
//#include "../containers/string.hpp"
#include <cstring>
#include <string> // TEMP (maybe)


namespace gvl
{

template<typename DerivedT>
struct basic_text_writer
{
	DerivedT& derived()
	{ return *static_cast<DerivedT*>(this); }
	
	DerivedT const& derived() const
	{ return *static_cast<DerivedT const*>(this); }
};


//void sequence(list<bucket>& l, std::size_t amount, linked_vector<uint8_t>& res);

// Provides functions for extracting data
// from a stream in a convenient and
// efficient manner.
// NOTE: You are not allowed to modify buckets
// that are buffered.
struct octet_stream_reader : gvl::shared
{
	typedef bucket::size_type size_type;
	
	octet_stream_reader(shared_ptr<stream> source_init)
	: cur_(0)
	, end_(0)
	, source_(source_init)
	{
	}
	
	octet_stream_reader()
	: cur_(0)
	, end_(0)
	, source_()
	{
	}
	
/*
	// Different naming to avoid infinite recursion if
	// not defined in DerivedT.
	bucket_source* get_source()
	{ return derived()->source(); }
	*/
	
	/*
	shared_ptr<stream> get_source()
	{ return source_; }*/
	
	uint8_t get()
	{
		// We keep this function small to encourage
		// inlining
		return (cur_ != end_) ? (*cur_++) : underflow_get_();
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
			if(len == 0)
				break;

			stream::read_status status = next_bucket_(uint32_t(len));
			if(status != stream::read_ok)
				throw stream_read_error(status, "Read error in get()");
		}
	}

	stream::read_status try_skip()
	{
		if(cur_ != end_)
		{
			++cur_;
			return stream::read_ok;
		}
		else
		{
			uint8_t dummy;
			return underflow_get_(dummy);
		}

	}
	
	stream::read_status try_get(uint8_t& ret)
	{
		if(cur_ != end_)
		{
			ret = *cur_++;
			return stream::read_ok;
		}
		else
		{
			return underflow_get_(ret);
		}
	}
	
	std::size_t try_get(uint8_t* dest, std::size_t len)
	{
		// TODO: Can optimize this
		for(std::size_t i = 0; i < len; ++i)
		{
			stream::read_status s = try_get(dest[i]);
			if(s != stream::read_ok)
			{
				return i;
			}
		}

		return len;
	}

	std::size_t try_skip(std::size_t len)
	{
		// TODO: Can optimize this
		for(std::size_t i = 0; i < len; ++i)
		{
			stream::read_status s = try_skip();
			if(s != stream::read_ok)
			{
				return i;
			}
		}

		return len;
	}

	// Try to buffer //len// bytes.
	stream::read_status try_buffer(std::size_t len)
	{
		if(first_left() >= len)
			return stream::read_ok; // Already buffered
		else
			return underflow_try_buffer_(len);
	}

	stream::read_status underflow_try_buffer_(std::size_t len)
	{
		// TODO: This is quite crude, but it's not expected to be used a lot.
		// TODO: Exception safety
		std::size_t found_amount = first_left();

		std::vector<bucket*> stack;
		stream::read_status s = stream::read_ok;

		while(found_amount < len)
		{
			stream::read_result r = read_bucket_and_return_(len - found_amount);

			if(r.s != stream::read_ok)
			{
				s = r.s;
				break;
			}

			found_amount += r.b->size();
			stack.push_back(r.b);
		}

		// Put back in reverse order

		while(!stack.empty())
		{
			source_->unread(stack.back());
			stack.pop_back();
		}

		return s;
	}
		
	bool at_eos()
	{
		if(cur_ != end_)
			return false;
			
		stream::read_status status = next_bucket_();
		if(status == stream::read_eos)
			return true;
	
		return false; // TODO: Does erroring count as eos?	
	}
	
	// TODO: A get that returns a special value for EOF

	// TODO: This returned an auto_read_result before
	stream::read_result get_bucket(size_type amount = 0);
	
	// Non-blocking
	stream::read_result try_get_bucket(size_type amount = 0);
	
	shared_ptr<stream> detach();

	stream& source()
	{ return *source_; }

	bool has_source() const
	{ return source_.get() != 0; }
	
	void attach(shared_ptr<stream> source_new)
	{
		if(source_)
			throw stream_error("A source is already attached to this octet_stream_reader");
		
		source_ = source_new;
	}

	void seekg(uint64_t pos)
	{
		shared_ptr<stream> str(detach());
		str->seekg(pos);
		attach(str);
	}
	
	/// Amount of data left in the first bucket
	std::size_t first_left() const { return end_ - cur_; }
	
private:
	
	uint8_t underflow_get_();
	stream::read_status underflow_get_(uint8_t& ret);
	
	/// Discards the current first bucket (if any) and tries to read
	/// a bucket if necessary.
	/// May throw.
	/// Precondition: cur_ == end_
	stream::read_status next_bucket_(uint32_t amount = 0);
	
	void check_source()
	{
		if(!source_)
			throw stream_read_error(stream::read_error, "No source assigned to octet_stream_reader");
	}

	
		
	// May throw
	stream::read_result read_bucket_and_return_(size_type amount);
	
	// May throw
	stream::read_result try_read_bucket_and_return_(size_type amount)
	{
		check_source();
		return source_->read(amount);
	}
		
	/// Apply changes to first bucket
	void correct_first_bucket_()
	{
		if(first_.get())
		{
			std::size_t old_size = first_->size();
			first_->cut_front(old_size - first_left());
		}
	}
	
	void set_first_bucket_(bucket* b)
	{
		//passert(!first_.get(), "Still a bucket in first_");
		size_type s = b->size();
		
		first_.reset(b);
		// New first bucket, update cur_ and end_
		cur_ = b->get_ptr();
		end_ = cur_ + s;
	}
	
	uint8_t const* cur_; // Pointer into first_
	uint8_t const* end_; // End of data in first_
	std::auto_ptr<bucket> first_;
	shared_ptr<stream> source_;
};

struct brigade;

struct octet_stream_writer
	: basic_text_writer<octet_stream_writer>
	, gvl::shared
{
	enum
	{
		default_initial_bucket_size = 512,
		default_max_bucket_size = 32768
	};
	
	
	octet_stream_writer(shared_ptr<stream> sink)
	: sink_()
	, cur_(0)
	, end_(0)
	, cap_(0)
	, buffer_()
	, estimated_needed_buffer_size_(default_initial_bucket_size)
	, max_bucket_size(default_max_bucket_size)
	{
		attach(sink);
	}
	
	octet_stream_writer()
	: sink_()
	, cur_(0)
	, end_(0)
	, cap_(0)
	, buffer_()
	, estimated_needed_buffer_size_(default_initial_bucket_size)
	, max_bucket_size(default_max_bucket_size)
	{
	}
	
	~octet_stream_writer()
	{
		if(sink_)
			flush();
	}
	
	stream::write_status flush(bucket_size new_buffer_size = 0);
	stream::write_status weak_flush(bucket_size new_buffer_size = 0);
	//stream::write_status partial_flush();

	stream::write_status put(uint8_t b)
	{
		// We keep this function small to encourage
		// inlining of the common case
		return (cur_ != end_) ? (*cur_++ = b, stream::write_ok) : overflow_put_(b);
	}
	
	stream::write_status put(uint8_t const* p, std::size_t len)
	{
		// We keep this function small to encourage
		// inlining of the common case
		if(std::size_t(end_ - cur_) >= len)
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
					
				return stream::write_ok;
			}
#endif
			std::memcpy(cur_, p, len);
			cur_ += len;
			return stream::write_ok;
		}
		else
		{
			return overflow_put_(p, len);
		}
	}
	
	stream::write_status put(uint32_t const* p, std::size_t len)
	{
		stream::write_status res = stream::write_ok;
		for(std::size_t i = 0; i < len; ++i)
		{
			res = stream::combine_write_status(res, put(*p++));
		}
		return res;	
	}
		
	stream::write_status put_bucket(bucket* buf);
	
	shared_ptr<stream> detach()
	{
		if(has_sink())
		{
			flush_buffer();
		
			// Buffer any remaining buckets
			// partial_flush already does this: sink_->write_buffered(mem_buckets_);
		}
		
		return sink_.release();
	}

	stream& sink()
	{ return *sink_; }

	bool has_sink() const
	{ return sink_.get() != 0; }
	
	void attach(shared_ptr<stream> new_sink)
	{
		if(sink_)
			throw stream_error("A sink is already attached to the octet_stream_writer");
		sink_ = new_sink;
		cap_ = default_initial_bucket_size;
		buffer_.reset(bucket_data_mem::create(cap_, 0));
		read_in_buffer_();
	}
	
	void check_sink()
	{
		if(!sink_)
			throw stream_write_error(stream::write_error, "No sink assigned to octet_stream_writer");
	}
	
	void swap(octet_stream_writer& b)
	{
		gvl::shared::swap(b);
		sink_.swap(b.sink_);
		std::swap(cur_, b.cur_);
		std::swap(end_, b.end_);
		std::swap(cap_, b.cap_);
		
		{ // auto_ptr doesn't have swap, so we need to do this
			std::auto_ptr<bucket_data_mem> tmp = buffer_;
			buffer_ = b.buffer_;
			b.buffer_ = tmp;
		}
		std::swap(estimated_needed_buffer_size_, b.estimated_needed_buffer_size_);
	}
	
	void reserve(std::size_t size)
	{
		ensure_cap_(size);
	}
	
	// Make the growth of the current bucket unlimited
	void set_unlimited_bucket()
	{
		max_bucket_size = std::size_t(0) - 1;
	}
		
private:
	stream::write_status flush_buffer(bucket_size new_buffer_size = 0);
	
	std::size_t buffer_size_()
	{
		return (cur_ - buffer_->data);
	}
	
	void correct_buffer_()
	{
		buffer_->size_ = buffer_size_();
	}
	
	void read_in_buffer_()
	{
		cur_ = buffer_->data + buffer_->size_;
		end_ = buffer_->data + cap_;
	}
	
	stream::write_status overflow_put_(uint8_t b);
	
	stream::write_status overflow_put_(uint8_t const* p, std::size_t len);
	
	void ensure_cap_(std::size_t s);

	//bucket_size size_;
	shared_ptr<stream> sink_;
	uint8_t* cur_; // Pointer into buffer_
	uint8_t* end_; // End of capacity in buffer_
	bucket_size cap_;
	//list<bucket> mem_buckets_;
	std::auto_ptr<bucket_data_mem> buffer_;
	bucket_size estimated_needed_buffer_size_;
	bucket_size max_bucket_size;
};

/*
template<typename Writer = octet_stream_writer>
struct raw_ansi_format_writer : basic_text_writer<raw_ansi_format_writer<Writer> >
{
	raw_ansi_format_writer(Writer& w_init)
	: w(w_init)
	{
	}
	
	void flush()
	{ w.flush(); }
	
	void put(uint32_t c)
	{ w.put((uint8_t)c); }
	
	void put(uint8_t const* b, std::size_t length)
	{ w.put(b, length); }
			
	Writer& w;
};*/

template<typename D>
inline D& operator<<(basic_text_writer<D>& self_, uint32_t x)
{
	D& self = self_.derived();
	uint_to_ascii_base<10>(self, x);
	return self;
}

template<typename D>
inline D& operator<<(basic_text_writer<D>& self_, int32_t x)
{
	D& self = self_.derived();
	int_to_ascii_base<10>(self, x);
	return self;
}

template<typename D>
inline D& operator<<(basic_text_writer<D>& self_, char const* str)
{
	D& self = self_.derived();
	self.put(reinterpret_cast<uint8_t const*>(str), std::strlen(str));
	return self;
}

template<typename D>
inline D& operator<<(basic_text_writer<D>& self_, char ch)
{
	D& self = self_.derived();
	self.put(static_cast<uint8_t>(ch));
	return self;
}

template<typename D>
inline D& operator<<(basic_text_writer<D>& self_, std::string const& str)
{
	D& self = self_.derived();
	self.put(reinterpret_cast<uint8_t const*>(str.data()), str.size());
	return self;
}

/*
template<typename D>
inline D& operator<<(basic_text_writer<D>& self_, gvl::string const& str)
{
	D& self = self_.derived();
	self.put(reinterpret_cast<uint8_t const*>(str.data()), str.size());
	return self;
}*/

template<typename D>
inline D& operator<<(basic_text_writer<D>& self_, void const* ptr)
{
	// TODO: Very TEMP
	return (self_ << uint32_t(ptr));
}

#if 0
template<typename T, typename Format>
struct with_format_
{
	with_format_(T const& obj, Format const& f)
	: obj(obj), f(f)
	{
	}

	T const& obj;
	Format const& f;
};

template<typename T, typename Format>
inline with_format_<T, Format> with_f(T const& obj, Format const& f)
{
	return with_format_<T, Format>(obj, f);
}

template<typename D, typename T, typename Format>
inline D& operator<<(basic_text_writer<D>& self_, with_format_<T, Format> wf)
{
	return self_ << wf.f(wf.obj);
}
#endif



struct endl_tag_ {};
inline void endl(endl_tag_) {}

template<typename D>
inline D& operator<<(basic_text_writer<D>& self_, void (*)(endl_tag_))
{
	D& self = self_.derived();
	self.put('\n');
	self.flush();
	return self;
}

struct cell : basic_text_writer<cell>
{
	enum placement
	{
		left, center, right
	};

	cell()
	: text_placement(left)
	, width(-1)
	{
	}

	cell(placement text_placement_init)
	: text_placement(text_placement_init)
	, width(-1)
	{
	}
	
	cell(int width_init, placement text_placement_init)
	: text_placement(text_placement_init)
	, width(width_init)
	{
	}
	
	void put(uint8_t x)
	{ buffer.push_back(x); }
	
	void put(uint8_t const* p, std::size_t len)
	{
		for(std::size_t i = 0; i < len; ++i)
		{
			buffer.push_back(p[i]);
		}
	}
	
	std::vector<uint8_t> buffer;
	placement text_placement;
	int width;
};
template<typename D>
inline D& operator<<(basic_text_writer<D>& self_, cell& c)
{
	D& self = self_.derived();
	if(c.buffer.size() > c.width)
	{
		int allowed = std::max(int(c.buffer.size()) - 2, 0);
		self.put(&c.buffer[0], &c.buffer[0] + allowed);
		if(allowed != int(c.buffer.size()))
			self << "..";
	}
	return self;
}

}

#endif // UUID_ADEA58A2C81F42C25E8CAFA32ED72A18
