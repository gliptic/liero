#ifndef UUID_957E3642BB06466DB21A21AFD72FAFAF
#define UUID_957E3642BB06466DB21A21AFD72FAFAF

#include "../containers/list.hpp"
#include "../containers/bucket.hpp"
//#include "../containers/string.hpp"
#include "../support/debug.hpp"
#include "../support/cstdint.hpp"
#include "../support/platform.hpp"
#include "../resman/shared_ptr.hpp"
#include <memory>
#include <vector>
#include <new>
#include <stdexcept>
#include <string>
#include <cstring>

#include <cstdio> // TEMP

namespace gvl
{

struct bucket;
struct brigade;



struct stream_error : std::runtime_error
{
	stream_error(std::string const& msg)
	: std::runtime_error(msg)
	{
	}
};



struct brigade
{
	typedef bucket_size size_type;
	
	/*
	read_result read(size_type amount = 0, bucket* dest = 0)
	{
		list<bucket>::iterator i = buckets.begin();
		
		if(i == buckets.end())
			return read_result(eos);
			
		read_result r(i->read(amount, 0));
		if(r.s == bucket_source::ok)
			buckets.unlink(r.b); // Success, we may unlink the bucket
			
		return r;
	}*/

	void prepend(bucket* b)
	{
		buckets.relink_front(b);
	}
	
	void append(bucket* b)
	{
		buckets.relink_back(b);
	}
	
	bool empty() const
	{
		return buckets.empty();
	}
	
	bucket* first()
	{
		return buckets.first();
	}
	
	bucket* unlink_first()
	{
		bucket* ret = buckets.first();
		buckets.unlink_front();
		return ret;
	}
	
	bucket* unlink_last()
	{
		bucket* ret = buckets.last();
		buckets.unlink_back();
		return ret;
	}
	
	list<bucket> buckets;
};

struct stream : shared
{
	typedef bucket_size size_type;
	
	enum read_status
	{
		read_ok = 0,
		read_blocking,
		read_eos,
		read_error,
		
		read_none_done // Special for filters
	};
	
	enum write_status
	{
		write_ok = 0,
		write_part,
		write_would_block,
		write_error
	};
	
	enum state
	{
		state_open,
		state_closed
	};
	
	struct read_result
	{
		explicit read_result(read_status s)
		: s(s)
		, b(0)
		{
		}
		
		read_result(read_status s, bucket* b)
		: s(s)
		, b(b)
		{
		}
		
		read_status s;
		bucket* b;
	};
	
	struct write_result
	{
		explicit write_result(write_status s, bool consumed)
		: s(s)
		, consumed(consumed)
		{
		
		}
		
		write_status s;
		bool consumed;
	};
	
	static write_status combine_write_status(write_status a, write_status b)
	{
		if(a == b)
			return a;
		else if(a == write_ok)
			return b;
		else if(b == write_ok)
			return a;
		else
			return write_error;
	}
	
	stream()
	: cur_state(state_open)
	{
	}
	
	~stream()
	{
		// We don't close here, because derived objects are already destroyed
	}
	
	virtual read_result read_bucket(size_type amount = 0, bucket* dest = 0) = 0;
	/*
	{
		return read_result(read_blocking);
	}*/
	
	read_result read(size_type amount = 0)
	{
		if(!in_buffer.empty())
			return read_result(read_ok, in_buffer.unlink_first());

		return read_bucket(amount);
	}
	
	// Unread buckets so that subsequent calls to read will
	// return them.
	void unread(list<bucket>& buckets)
	{
		in_buffer.buckets.splice_front(buckets);
	}
	
	void unread(bucket* b)
	{
		in_buffer.buckets.relink_front(b);
	}
	
	/// Writes a bucket to a sink. If bucket_sink::ok is returned,
	/// takes ownership of 'b' and unlinks it from it's list<bucket>.
	/// NOTE: 'b' must be inserted into a list<bucket> or be a singleton.
	virtual write_result write_bucket(bucket* b) = 0;
	/*
	{
		return write_result(write_would_block, false);
	}*/
	
	write_result write(bucket* b)
	{
		write_result res = flush_out_buffer();
		if(!res.consumed)
			return res;

		return write_bucket(b);
	}
	
	/// NOTE: 'b' must be inserted into a list<bucket> or be a singleton.
	write_result write_or_buffer(bucket* b)
	{
		write_result res = write_bucket(b);
		if(!res.consumed)
			write_buffered(b);
		res.consumed = true;
		return res;
	}
	
	// Buffer a list of buckets.
	void write_buffered(list<bucket>& buckets)
	{
		out_buffer.buckets.splice(buckets);
	}
		
	/// NOTE: 'b' must be inserted into a list<bucket> or be a singleton.
	void write_buffered(bucket* b)
	{
		gvl::unlink(b);
		out_buffer.buckets.relink_front(b);
	}
	
	write_status flush()
	{
		write_result res = flush_out_buffer();
		if(!res.consumed)
			return res.s;
		
		return propagate_flush();
	}
	
	write_result flush_out_buffer()
	{
		while(!out_buffer.empty())
		{
			bucket* buffered = out_buffer.first();
			write_result res = write_bucket(buffered);
			if(!res.consumed)
				return res;
		}
		
		return write_result(write_ok, true);
	}
	
	// NOTE! This may NEVER throw!
	write_status close()
	{
		if(cur_state != state_open)
			return write_ok;
			
		cur_state = state_closed;
			
		write_result res = flush_out_buffer();
		if(!res.consumed)
			return res.s;
		
		return propagate_close();
	}
		
	/// This is supposed to propagate a flush to
	/// the underlying sink. E.g. in a filter, it would
	/// propagate the flush to the connected sink.
	virtual write_status propagate_flush()
	{
		return write_ok;
	}
	
	virtual write_status propagate_close()
	{
		return write_ok;
	}

	virtual read_status seekg(uint64_t pos)
	{
		// Not supported by default
		return read_error;
	}
	
	brigade in_buffer;
	brigade out_buffer;
	state cur_state;
};


struct stream_read_error : stream_error
{
	stream_read_error(stream::read_status s, std::string const& msg)
	: stream_error(msg), s(s)
	{
	}
	
	stream::read_status s;
};

struct stream_write_error : stream_error
{
	stream_write_error(stream::write_status s, std::string const& msg)
	: stream_error(msg), s(s)
	{
	}
	
	stream::write_status s;
};

typedef shared_ptr<stream> stream_ptr;



/*
inline octet_stream_writer& operator<<(octet_stream_writer& writer, char const* str)
{
	std::size_t len = std::strlen(str);
	writer.put(reinterpret_cast<uint8_t const*>(str), len);
	return writer;
}

inline octet_stream_writer& operator<<(octet_stream_writer& writer, std::string const& str)
{
	writer.put(reinterpret_cast<uint8_t const*>(str.data()), str.size());
	return writer;
}*/

/*
/// A sink that forwards to a brigade
template<typename DerivedT, brigade& (DerivedT::*Get)()>
struct basic_brigade_sink : bucket_sink
{
	brigade& get()
	{ return (static_cast<DerivedT*>(this)->*Get)(); }
	
	bucket_sink::status write(bucket* b)
	{
		b->unlink();
		get().append(b);
		return bucket_sink::ok;
	}
};
*/



struct filter : stream
{
	struct pump_result
	{
		pump_result(read_status r, write_status w)
		: r(r), w(w)
		{
		}
		
		read_status r;
		write_status w;
	};
	
	enum apply_mode
	{
		am_non_pulling,
		am_pulling,
		am_flushing,
		am_closing
	};
	
	filter()
	{
	}
		
	filter(shared_ptr<stream> source_init, shared_ptr<stream> sink_init)
	: source(source_init)
	, sink(sink_init)
	{
	}
	
	read_result read_bucket(size_type amount = 0, bucket* dest = 0)
	{
		if(!source)
			return read_result(read_error);
		
		read_status status = apply(am_pulling, amount);
		if(status != read_ok)
			return read_result(status);
		read_result res(read_ok, in_buffer.unlink_first());
		return res;
	}
	
	write_result write_bucket(bucket* b)
	{
		if(!sink)
			return write_result(write_error, false);
		
		unlink(b);
		filter_buffer.append(b);
		apply(am_non_pulling);
		write_status rstatus = flush_filtered();
		if(rstatus != write_ok)
		{
			return write_result(rstatus, true);
		}
		
		return write_result(write_ok, true);
	}
	
	
	
	write_status propagate_flush()
	{
		sassert(out_buffer.empty()); // stream should have taken care of this
		
		// We don't check sink here so that
		// we only error on missing sink if there's actually anything
		// left to write.
		
		if(!sink)
		{
			if(!out_buffer.empty() || !filter_buffer.empty())
				return write_error; // Still data to filter or not written
		}
		else
		{
			apply(am_flushing);
			write_status res = flush_filtered();
			if(res != write_ok)
				return res;
			if(!filter_buffer.empty())
				return write_would_block; // Still data that has not been filtered
		}
		return write_ok;
	}
	
	write_status propagate_close()
	{
		sassert(out_buffer.empty());
		
		if(!sink)
		{
			if(!out_buffer.empty() || !filter_buffer.empty())
				return write_error; // Still data to filter or not written
		}
		else
		{
			apply(am_closing);
			write_status res = flush_filtered();
			if(res != write_ok)
				return res;
			if(!filter_buffer.empty())
				return write_would_block; // Still data that has not been filtered
		}
		return write_ok;
	}
	
	pump_result pump()
	{
		if(!source)
			return pump_result(read_error, write_error);
		if(!sink)
			return pump_result(read_error, write_error);
			
		if(in_buffer.empty())
		{
			read_status rstatus = apply(am_pulling);
			if(rstatus != read_ok)
				return pump_result(rstatus, write_ok);
		}
		write_status wstatus = flush_filtered();
		return pump_result(read_ok, wstatus);
	}
	
	void attach_source(shared_ptr<stream> source_new)
	{
		source = source_new;
	}
	
	void attach_sink(shared_ptr<stream> sink_new)
	{
		sink = sink_new;
	}
		
protected:

	// Preconditions: sink
	write_status flush_filtered()
	{
		sassert(sink); // Precondition
		
		while(!in_buffer.empty())
		{
			write_result res = sink->write(in_buffer.first());
			if(res.s != write_ok)
			{
				return res.s;
			}
		}
		
		return write_ok;
	}
	
	

	// Filter buckets in filter_buffer and append the result to in_buffer.
	// If mode is flushing or closing, the filter should make every effort to
	// filter all buckets in filter_buffer.
	// 
	// If mode is pulling, the filter should make some effort to produce
	// at least one filtered bucket.
	// 
	// TODO: Return value of this function is quite useless at the moment.
	virtual read_status apply(apply_mode mode, size_type amount = 0)
	{
		// We bypass filter_buffer
		if(!out_buffer.empty())
		{
			in_buffer.buckets.splice(out_buffer.buckets);
			return read_ok;
		}
		else if(mode == am_pulling)
		{
			read_result res = source->read(amount);
			if(res.s == read_ok)
				in_buffer.append(res.b);
			return res.s;
		}
		else
			return read_blocking;
	}
	
	read_status try_pull(size_type amount = 0)
	{
		read_result res = source->read(amount);
		if(res.s == read_ok)
			filter_buffer.append(res.b);
		return res.s;
	}
		
	shared_ptr<stream> source;
	shared_ptr<stream> sink;

	brigade filter_buffer;
};

typedef shared_ptr<filter> filter_ptr;

struct memory_stream : stream
{
	read_result read_bucket(size_type amount = 0, bucket* dest = 0)
	{
	/* stream::read already checked in_buffer
		if(!buffer.empty())
		{
			read_result res(read_ok, buffer.first());
			unlink(res.b);
			return res;
		}
		*/
		return read_result(read_blocking);
	}
	
	write_result write_bucket(bucket* b)
	{
		unlink(b);
		in_buffer.append(b);
		return write_result(write_ok, true);
	}
	
	void clear()
	{
		in_buffer.buckets.clear();
	}
	
	void to_str(std::string& ret)
	{
		ret.clear();
		for(list<bucket>::iterator i = in_buffer.buckets.begin(); i != in_buffer.buckets.end(); ++i)
		{
			char const* p = reinterpret_cast<char const*>(i->get_ptr());
			ret.insert(ret.end(), p, p + i->size());
		}
	}
	
	/*
	template<std::size_t InlineSize, bool Cow>
	void release_as_str(gvl::basic_string<InlineSize, Cow>& ret)
	{
		if(in_buffer.buckets.empty())
		{
			ret.clear();
			return;
		}
		list<bucket>::iterator i = in_buffer.buckets.begin();
		
		// TODO: The data usually has more capacity than 'size',
		// but this information is lost.
		std::size_t size = i->size();
		if(i->bucket_begins_at_zero())
			ret.assign(i->release_data(), size, size);
		else
			ret.assign(i->get_ptr(), size);
			
		while(++i != in_buffer.buckets.end())
		{
			std::size_t size = i->size();
			ret.append(i->get_ptr(), size);
		}
		
		in_buffer.buckets.clear();
	}*/
};

template<typename Writer>
inline void write_uint16(Writer& writer, unsigned int v)
{
	sassert(v < 0x10000);
	writer.put(uint8_t((v >> 8) & 0xff));
	writer.put(uint8_t(v & 0xff));
}

template<typename Writer>
inline void write_sint16(Writer& writer, unsigned int v)
{
	write_uint16(writer, v + 0x8000);
}

template<typename Writer>
inline void write_uint16_le(Writer& writer, unsigned int v)
{
	sassert(v < 0x10000);
	writer.put(uint8_t(v & 0xff));
	writer.put(uint8_t((v >> 8) & 0xff));
}

template<typename Writer>
inline void write_uint24(Writer& writer, unsigned int v)
{
	sassert(v < 0x1000000);
	writer.put(uint8_t((v >> 16) & 0xff));
	writer.put(uint8_t((v >> 8) & 0xff));
	writer.put(uint8_t(v & 0xff));
}

template<typename Writer>
inline void write_uint24_le(Writer& writer, unsigned int v)
{
	sassert(v < 0x1000000);
	writer.put(uint8_t(v & 0xff));
	writer.put(uint8_t((v >> 8) & 0xff));
	writer.put(uint8_t((v >> 16) & 0xff));
}

template<typename Writer>
inline void write_uint32(Writer& writer, uint32_t v)
{
	writer.put(uint8_t((v >> 24) & 0xff));
	writer.put(uint8_t((v >> 16) & 0xff));
	writer.put(uint8_t((v >> 8) & 0xff));
	writer.put(uint8_t(v & 0xff));
}

template<typename Writer>
inline void write_uint32_le(Writer& writer, uint32_t v)
{
	writer.put(uint8_t(v & 0xff));
	writer.put(uint8_t((v >> 8) & 0xff));
	writer.put(uint8_t((v >> 16) & 0xff));
	writer.put(uint8_t((v >> 24) & 0xff));
}

template<typename Writer>
inline void write_sint32(Writer& writer, int v)
{
	write_uint32(writer, v + 0x80000000);
}

template<typename Writer>
void write_string16(Writer& writer, std::string const& src)
{
	int len = src.size();
	if(len > 65535)
		len = 65535;
	write_uint16(writer, len);
	writer.put(reinterpret_cast<uint8_t const*>(src.data()), len);
}

template<typename Writer>
void write_aint(Writer& writer, unsigned int v)
{
	while(true)
	{
		int b = v & 0x7f;
		v >>= 7;
			
		if(v != 0)
			writer.put(b | 0x80);
		else
		{
			writer.put(b);
			break;
		}
	}
}

template<typename Reader>
unsigned int read_aint(Reader& reader)
{
	v = 0;

	for(int i = 0; i < 5; ++i)
	{
		uint8_t b = reader.get();

		if((b & 0x80) == 0)
		{
			v |= b;
			return v; // Ok
		}
		else
		{
			v = (v << 7) | (b & 0x7f);
		}
	}
		
	throw gvl::stream_error("Malformed aint in read_aint");
}

template<typename Reader>
inline unsigned int read_uint24(Reader& reader)
{
	unsigned int ret = reader.get() << 16;
	ret |= reader.get() << 8;
	ret |= reader.get();
	return ret;
}

template<typename Reader>
inline unsigned int read_uint24_le(Reader& reader)
{
	unsigned int ret = reader.get();
	ret |= reader.get() << 8;
	ret |= reader.get() << 16;
	return ret;
}

template<typename Reader>
inline unsigned int read_uint16(Reader& reader)
{
	unsigned int ret = reader.get() << 8;
	ret |= reader.get();
	return ret;
}

template<typename Reader>
inline unsigned int read_uint16_le(Reader& reader)
{
	unsigned int ret = reader.get();
	ret |= reader.get() << 8;
	return ret;
}

template<typename Reader>
inline uint32_t read_uint32(Reader& reader)
{
	unsigned int ret = reader.get() << 24;
	ret |= reader.get() << 16;
	ret |= reader.get() << 8;
	ret |= reader.get();
	return ret;
}

template<typename Reader>
inline uint32_t read_uint32_le(Reader& reader)
{
	unsigned int ret = reader.get();
	ret |= reader.get() << 8;
	ret |= reader.get() << 16;
	ret |= reader.get() << 24;
	return ret;
}

template<typename Reader>
inline uint64_t read_uint64_le(Reader& reader)
{
	uint64_t ret = reader.get();
	ret |= uint64_t(reader.get()) << 8;
	ret |= uint64_t(reader.get()) << 16;
	ret |= uint64_t(reader.get()) << 24;
	ret |= uint64_t(reader.get()) << 32;
	ret |= uint64_t(reader.get()) << 40;
	ret |= uint64_t(reader.get()) << 48;
	ret |= uint64_t(reader.get()) << 56;
	return ret;
}



template<typename Reader>
inline int read_sint16(Reader& reader)
{
	return read_uint16(reader) - 0x8000;
}

template<typename Reader>
inline int read_sint32(Reader& reader)
{
	return read_uint32(reader) - 0x80000000;
}

template<typename Reader>
void read_string16(Reader& reader, std::string& dest)
{
	unsigned int len = read_uint16(reader);
	std::vector<char> buf(len + 1); // + 1 to avoid zero-length buf that would make buf[0] undefined
	reader.get(reinterpret_cast<uint8_t*>(&buf[0]), len);

	dest.assign(&buf[0], len);
}

}

#endif // UUID_957E3642BB06466DB21A21AFD72FAFAF
