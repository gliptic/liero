#ifndef GVL_BUCKET_HPP
#define GVL_BUCKET_HPP

#include <new>
#include <cstddef>
#include <cstring>
#include "../containers/list.hpp"
#include "../resman/shared_ptr.hpp"
#include "../support/cstdint.hpp"

namespace gvl
{

typedef std::size_t bucket_size;

struct bucket;

struct bucket_data : shared
{
	typedef bucket_size size_type;
	static size_type const nsize = size_type(-1);
	
	bucket_data()
	{
	}
	
	virtual size_type size() const = 0;
	
	virtual uint8_t const* get_ptr(bucket& owner_bucket, size_type offset) = 0;
	
	// Placement-new
	void* operator new(std::size_t, void* p)
	{
		return p;
	}
	
	// To shut up the compiler
	void operator delete(void*, void*)
	{
	}
	
	// Allocation is overloaded in order to allow arbitrary sized objects
	void* operator new(std::size_t n)
	{
		return new char[n];
	}
	
	void operator delete(void* m)
	{
		delete [] static_cast<char*>(m);
	}
};


struct bucket_data_mem : bucket_data
{	
	static std::size_t compute_size(bucket_size n)
	{
		// We would normally subtract 1 from this to get it right.
		// However, we allocate one byte extra to fit the zero-
		// terminator in gvl::string.
		return sizeof(bucket_data_mem) + n*sizeof(uint8_t);
	}

	static bucket_data_mem* create(bucket_size size)
	{
		return create(size, size);
	}
	
	static bucket_data_mem* create(bucket_size capacity, bucket_size size)
	{
		sassert(size <= capacity);
		void* mem = new char[compute_size(capacity)];
		return new (mem) bucket_data_mem(size);
	}
	
	static bucket_data_mem* create_from(uint8_t const* b, uint8_t const* e, std::size_t cap_init)
	{
		std::size_t s = e - b;
		sassert(cap_init >= s);
		bucket_data_mem* ret = create(cap_init, s);
		std::memcpy(ret->data, b, s);
		return ret;
	}
	
	bucket_data_mem* clone(std::size_t cap_new, std::size_t size_new) const
	{
		bucket_data_mem* n = create(cap_new, size_new);
		std::memcpy(n->data, data, size_new);
		return n;
	}
	
	bucket_data_mem(bucket_size size)
	: size_(size)
	{
	}
	
	uint8_t const* get_ptr(bucket& owner_bucket, size_type offset)
	{
		return data + offset;
	}
	
	size_type size() const
	{
		return size_;
	}
	
	void unsafe_push_back(uint8_t el)
	{
		data[size_] = el;
		++size_;
	}
	
	void unsafe_push_back(uint8_t const* p, bucket_size len)
	{
		std::memcpy(&data[size_], p, len);
		size_ += len;
	}
		
	bucket_data_mem* enlarge(bucket_size n)
	{
		bucket_data_mem* new_data = create(n, size_);
		std::memcpy(new_data->data, data, size_);
		return new_data;
	}
	
	uint8_t* begin()
	{ return data; }
	
	uint8_t* end()
	{ return data + size_; }
	
	std::size_t size_;
	uint8_t data[1];
};

typedef shared_ptr<bucket_data_mem> data_buf_ptr;

struct bucket : list_node<>
{
	typedef bucket_size size_type;
	
	bucket()
	: begin_(0)
	, end_(-1)
	{
	}
	
	bucket(bucket const& b, std::size_t begin, std::size_t end)
	: data_(b.data_)
	, begin_(begin)
	, end_(end)
	{
		
	}
	
	bucket(bucket_data_mem* data)
	: data_(data)
	, begin_(0)
	, end_(0)
	{
		end_ = data_->size();
		if(end_ == bucket_data::nsize)
		{
			end_ = 0;
			begin_ = 1;
		}
	}
	
	bucket(bucket_data_mem* data, size_type begin, size_type end)
	: data_(data)
	, begin_(begin)
	, end_(end)
	{
	}
	
	bucket(void const* ptr, size_type len);
			
	bool size_known() const { return begin_ <= end_; }
	
	size_type begin() const
	{
		passert(size_known(), "Size is unknown");
		return begin_;
	}
	
	size_type end() const
	{
		passert(size_known(), "Size is unknown");
		return end_;
	}
	
	size_type size() const
	{
		passert(size_known(), "Size is unknown");
		return static_cast<size_type>(end_ - begin_);
	}
	
	void split(std::size_t point)
	{
		passert(size_known(), "Size is unknown");
		passert(0 <= point && point <= size(), "Split point is out of bounds");
		
		if(point == 0 || point == size())
			return; // No need to do anything
				
		// Insert before
		// TODO: We should actually let the bucket split itself.
		// It should at least know when it's being copied.
		relink_after(this, new bucket(*this, begin_ + point, end_));
		end_ -= (size() - point);
	}
	
	inline uint8_t const* get_ptr();
		
	void cut_front(size_type amount)
	{
		passert(size_known(), "Size is unknown");
		begin_ += amount;
		passert(begin_ <= end_, "Underflow");
	}
	
	void cut_back(size_type amount)
	{
		passert(size_known(), "Size is unknown");
		end_ -= amount;
		passert(begin_ <= end_, "Underflow");
	}
	
	shared_ptr<bucket_data_mem> release_data()
	{
		begin_ = 0;
		end_ = 0;
		return data_.release();
	}

	bool bucket_begins_at_zero() const
	{
		return begin_ == 0;
	}
		
	bucket* clone() const
	{
		return new bucket(*this, begin_, end_);
	}
	
	~bucket()
	{
	}
	
	
protected:

	shared_ptr<bucket_data_mem> data_;
	std::size_t begin_;
	std::size_t end_;
};


uint8_t const* bucket::get_ptr()
{
	uint8_t const* ptr = data_->get_ptr(*this, begin_);
	//passert(dynamic_cast<bucket_data_mem*>(data_.get()), "get_ptr must replace the bucket_data with bucket_data_mem");
	return ptr;
}

}

#endif // GVL_BUCKET_HPP
