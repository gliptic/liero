#ifndef UUID_D5923885C48B49588EAD94A276B131F0
#define UUID_D5923885C48B49588EAD94A276B131F0

#include <iterator>
#include <cstddef>
#include <algorithm>
#include <list>
#include "../support/functional.hpp"
#include "../support/debug.hpp"
#include "list.h"

// BIG TODO: Exception safety

namespace gvl
{

struct default_list_tag
{};

/*
struct gvl_list_node : gvl_list_node
{
	gvl_list_node()
	{
		next = this;
		prev = this;
	}
	
	
	void unlink()
	{
		gvl_list_unlink(this);
	}
	
	// NOTE: The behaviour of relink is only defined if the
	// nodes that were immediately left and right of this node when
	// it was unlinked are now consecutive (and in the same order).
	// This is (of course) the case right after the unlink.
	void relink()
	{
		passert(
			prev->next == next && next->prev == prev,
			"Adjacent nodes must be consecutive");
		prev->next = this;
		next->prev = this;
	}
	
	template<typename ListT>
	void move_to(ListT& dest);
};
*/

template<typename ListT>
inline void move_to(gvl_list_node* self, ListT& dest);

template<typename Tag>
inline void unlink(gvl_list_node* self);

// NOTE: The behaviour of relink is only defined if the
// nodes that were immediately left and right of this node when
// it was unlinked are now consecutive (and in the same order).
// This is (of course) the case right after the unlink.
inline void relink(gvl_list_node* self)
{
	passert(
		self->prev->next == self->next && self->next->prev == self->prev,
		"Adjacent nodes must be consecutive");
	self->prev->next = self;
	self->next->prev = self;
}

inline void unlink(gvl_list_node* self)
{
	gvl_list_unlink(self);
}

inline void relink(gvl_list_node* x, gvl_list_node* p)
{
	gvl_list_link_before(x, p);
}

inline void relink_before(gvl_list_node* x, gvl_list_node* p)
{
	gvl_list_link_before(x, p);
}

inline void relink_after(gvl_list_node* x, gvl_list_node* p)
{
	gvl_list_link_after(x, p);
}

/*
template<typename T, typename Op>
bool do_list_compare(void* comparer, gvl_list_node* a, gvl_list_node* b)
{
	Op& real_comparer = *static_cast<Op*>(comparer);
	return real_comparer(*static_cast<T*>(a), *static_cast<T*>(b));
}

typedef bool (*do_list_compare_func)(void*, gvl_list_node*, gvl_list_node*);
*/

template<typename Tag = default_list_tag>
struct list_node : gvl_list_node
{
	template<typename T>
	static T* downcast(gvl_list_node* p)
	{
		return static_cast<T*>(static_cast<list_node<Tag>*>(p));
	}
	
	list_node()
	{
		next = this;
		prev = this;
	}

	template<typename T>
	static list_node<Tag>* upcast(T* p)
	{
		return static_cast<list_node<Tag>*>(p);
	}
	
/*
	void relink(T* p)
	{
		gvl_list_link_before(this, upcast(p));
	}
	
	void relink_before(T* p)
	{
		gvl_list_link_before(this, upcast(p));
	}
	
	void relink_after(T* p)
	{
		gvl_list_link_after(this, upcast(p));
	}
	
	template<typename ListT>
	void move_to(ListT& dest)
	{
		gvl::move_to(this, dest);
	}*/
	
	/*
	T* next()
	{
		return static_cast<T*>(gvl_list_node::next);
	}

	T* prev()
	{
		return static_cast<T*>(gvl_list_node::prev);
	}*/
};

// NOTE: Be very careful with these! They assume the list you insert
// into hold a base class of T (or T itself).

template<typename Tag, typename T>
inline void unlink(T* self)
{
	gvl_list_unlink(list_node<Tag>::upcast(self));
}

template<typename Tag, typename T>
inline void relink(T* x, T* p)
{
	gvl_list_link_before(list_node<Tag>::upcast(x), list_node<Tag>::upcast(p));
}

template<typename Tag, typename T>
inline void relink_before(T* x, T* p)
{
	gvl_list_link_before(list_node<Tag>::upcast(x), list_node<Tag>::upcast(p));
}

template<typename Tag, typename T>
inline void relink_after(T* x, T* p)
{
	gvl_list_link_after(list_node<Tag>::upcast(x), list_node<Tag>::upcast(p));
}

template<typename Tag, typename T, typename ListT>
inline void move_to(T* x, ListT& dest)
{
	gvl::move_to(list_node<Tag>::upcast(x), dest);
}

struct list_common
{
	list_common()
	{
		sentinel_.next = &sentinel_;
		sentinel_.prev = &sentinel_;
	}

	gvl_list_node* relink_back(gvl_list_node* el)
	{
		gvl_list_link_before(&sentinel_, el);
		return el;
	}

	gvl_list_node* relink_front(gvl_list_node* el)
	{
		gvl_list_link_after(&sentinel_, el);
		return el;
	}

	gvl_list_node* unlink(gvl_list_node* i)
	{
		gvl_list_node* n = i->next;
		
		gvl::unlink(i);

		return n;
	}

	gvl_list_node* relink(gvl_list_node* b, gvl_list_node* el)
	{
		gvl_list_link_before(b, el);
		return el;
	}

	gvl_list_node* relink_after(gvl_list_node* b, gvl_list_node* el)
	{
		gvl_list_link_after(b, el);
		return el;
	}

	void swap(list_common& b)
	{
		gvl_list_node* a_first = sentinel_.next;
		gvl_list_node* a_last = sentinel_.prev;
		gvl_list_node* b_first = b.sentinel_.next;
		gvl_list_node* b_last = b.sentinel_.prev;
		
		// Order is important
		std::swap(a_first->prev, b_first->prev);
		std::swap(a_last->next, b_last->next);
		std::swap(sentinel_.next, b.sentinel_.next);
		std::swap(sentinel_.prev, b.sentinel_.prev);
	}
	
	bool empty() const
	{
		return sentinel_.next == &sentinel_;
	}

	std::size_t size() const;

	void unlink_all()
	{
		sentinel_.next = &sentinel_;
		sentinel_.prev = &sentinel_;
	}
	
	void integrity_check();

/*
	gvl_list_node* raw_first() const { return sentinel_.next; }
	gvl_list_node* raw_last() const { return sentinel_.prev; }
*/
	gvl_list_node* sentinel() { return &sentinel_; }

	//void sort(void* comparer, do_list_compare_func do_compare);
	
	gvl_list_node* first() const { return static_cast<gvl_list_node*>(sentinel_.next); }
	gvl_list_node* last() const { return static_cast<gvl_list_node*>(sentinel_.prev); }
	
	gvl_list_node sentinel_;
};

template<typename ListT>
inline void move_to(gvl_list_node* self, ListT& dest)
{
	unlink(self);
	dest.list_common::relink_back(self);
}

struct default_ownership
{
	template<typename T>
	T* take(T* p)
	{
		return p; // Verbatim
	}

/*
	template<typename T>
	T* release(T* p)
	{
		return p;
	}*/
};

template<
	typename T,
	typename Tag = default_list_tag,
	typename Deleter = default_delete,
	typename Ownership = default_ownership>
struct list : list_common, protected Deleter, protected Ownership
{
	static T* downcast(gvl_list_node* p)
	{
		return static_cast<T*>(static_cast<list_node<Tag>*>(p));
	}

	static list_node<Tag>* upcast(T* p)
	{
		return static_cast<list_node<Tag>*>(p);
	}

	struct iterator
	{
		typedef ptrdiff_t difference_type;
		typedef std::bidirectional_iterator_tag iterator_category;
		typedef T* pointer;
		typedef T& reference;
		typedef T value_type;
    
		iterator()
		: ptr_(0)
		{
		}

		iterator(gvl_list_node* ptr)
		: ptr_(ptr)
		{
		}
		
		iterator(T* ptr)
		: ptr_(upcast(ptr))
		{
		}

		T& operator*()
		{
			return *downcast(ptr_);
		}

		T* operator->()
		{
			return downcast(ptr_);
		}

		iterator& operator++()
		{
			ptr_ = ptr_->next;
			return *this;
		}

		iterator& operator--()
		{
			ptr_ = ptr_->prev;
			return *this;
		}
		
		iterator next() const
		{
			return iterator(ptr_->next);
		}
		
		iterator prev() const
		{
			return iterator(ptr_->prev);
		}

		bool operator==(iterator const& b)
		{
			return b.ptr_ == ptr_;
		}

		bool operator!=(iterator const& b)
		{
			return b.ptr_ != ptr_;
		}
		
		operator T*()
		{
			return downcast(ptr_);
		}

		gvl_list_node* ptr_;
	};
	
	struct reverse_iterator
	{
		typedef ptrdiff_t difference_type;
		typedef std::bidirectional_iterator_tag iterator_category;
		typedef T* pointer;
		typedef T& reference;
		typedef T value_type;
    
		reverse_iterator()
		: ptr_(0)
		{
		}

		reverse_iterator(gvl_list_node* ptr)
		: ptr_(ptr)
		{
		}
		
		reverse_iterator(T* ptr)
		: ptr_(upcast(ptr))
		{
		}

		T& operator*()
		{
			return *downcast(ptr_);
		}

		T* operator->()
		{
			return downcast(ptr_);
		}

		reverse_iterator& operator++()
		{
			ptr_ = ptr_->prev;
			return *this;
		}

		reverse_iterator& operator--()
		{
			ptr_ = ptr_->next;
			return *this;
		}
		
		reverse_iterator next() const
		{
			return reverse_iterator(ptr_->prev);
		}
		
		reverse_iterator prev() const
		{
			return reverse_iterator(ptr_->next);
		}

		bool operator==(reverse_iterator const& b)
		{
			return b.ptr_ == ptr_;
		}

		bool operator!=(reverse_iterator const& b)
		{
			return b.ptr_ != ptr_;
		}
		
		operator T*()
		{
			return downcast(ptr_);
		}

		gvl_list_node* ptr_;
	};

	struct range
	{
		range(gvl_list_node* front_init, gvl_list_node* end_init)
		: front_(front_init)
		, end_(end_init)
		{
		}

		bool empty()
		{ return front_ == end_; }

		void pop_front()
		{ front_ = front_->next; }

		T& front()
		{ return *downcast(front_); }

	private:
		gvl_list_node* front_;
		gvl_list_node* end_;
	};
	
	list(/*Deleter const& deleter = Deleter(), */Ownership const& ownership = Ownership())
	: /*Deleter(deleter)
	, */Ownership(ownership)
	{
	}
	
	/*
	list(list const& b)
	{
		// TODO: Add proper cloning semantics
		for(gvl_list_node* f = b.first(); f != &b.sentinel_; f = f->next)
		{
			push_back(new T(*downcast(f)));
		}
	}*/
	
	~list()
	{
		clear();
	}

	T* first() const
	{
		sassert(!empty());
		return downcast(sentinel_.next);
	}
	
	T* last() const
	{
		sassert(!empty());
		return downcast(sentinel_.prev);
	}
	
	iterator push_back(T* el)
	{
		el = static_cast<T*>(Ownership::take(el));
		return iterator(list_common::relink_back(upcast(el)));
	}
	
	iterator push_front(T* el)
	{
		el = static_cast<T*>(Ownership::take(el));
		return iterator(list_common::relink_front(upcast(el)));
	}
	
	iterator relink_back(T* el)
	{
		return iterator(list_common::relink_back(upcast(el)));
	}

	void pop_back()
	{
		erase(iterator(list_common::last()));
	}

	void pop_front()
	{
		erase(iterator(list_common::first()));
	}
	
	void unlink_back()
	{
		list_common::unlink(list_common::last());
	}
	
	void unlink_front()
	{
		list_common::unlink(list_common::first());
	}

	iterator erase(iterator i)
	{
		iterator n(list_common::unlink(i.ptr_));
		Deleter::operator()(downcast(i.ptr_));

		return n;
	}
	
	iterator erase(iterator b, iterator e)
	{
		iterator n;
		for(; b != e;)
		{
			n = list_common::unlink(b.ptr_);
			Deleter::operator()(downcast(b.ptr_));
			b = n;
		}

		return n; // NOTE: n should be equal to e
	}

	iterator begin()
	{
		return iterator(list_common::first());
	}

	iterator end()
	{
		return iterator(&sentinel_);
	}
	
	reverse_iterator rbegin()
	{
		return reverse_iterator(list_common::last());
	}

	reverse_iterator rend()
	{
		return reverse_iterator(&sentinel_);
	}

	range all()
	{
		return range(list_common::first(), &sentinel_);
	}

	T& front()
	{
		return *first();
	}

	T& back()
	{
		return *last();
	}
	
	bool is_end(T* p)
	{
		return upcast(p) == &sentinel_;
	}
	
	/*
	size_t size()
	{
		return list_common::size();
	}*/

	iterator unlink(T* i)
	{
		return iterator(list_common::unlink(upcast(i)));
	}

	void unlink_front(range& r)
	{
		T* f = &r.front();
		r.pop_front();
		unlink(f);
	}

	void erase_front(range& r)
	{
		T* f = &r.front();
		r.pop_front();
		erase(f);
	}
	
	iterator relink(iterator b, T* el)
	{
		return iterator(
			list_common::relink(
				b.ptr_,
				upcast(el)));
	}
	
	reverse_iterator relink(reverse_iterator b, T* el)
	{
		return reverse_iterator(
			list_common::relink_after(
				b.ptr_,
				upcast(el)));
	}
	
	iterator insert(iterator b, T* el)
	{
		el = static_cast<T*>(Ownership::take(el));
		return relink(b, el);
	}
	
	reverse_iterator insert(reverse_iterator b, T* el)
	{
		el = static_cast<T*>(Ownership::take(el));
		return relink(b, el);
	}
	
	template<typename Compare>
	iterator insert_sorted(T* el, Compare compare)
	{
		el = static_cast<T*>(Ownership::take(el));
		
		gvl_list_node* before = &sentinel_;
		gvl_list_node* after = before->next;
		while(after != &sentinel_ && compare(*downcast(after), *el))
		{
			before = after;
			after = after->next;
		}
		
		return iterator(list_common::relink(after, upcast(el)));
	}
	
	void move_to(iterator i, list& dest)
	{
		list_common::unlink(i.ptr_);
		dest.list_common::relink_back(i.ptr_);
	}

	void clear()
	{
		for(gvl_list_node* n = sentinel_.next; n != &sentinel_;)
		{
			gvl_list_node* next = n->next;
			Deleter::operator()(downcast(n));
			n = next;
		}

		list_common::unlink_all();
	}
		
	void splice(list& b)
	{
		if(!b.empty())
		{
			b.sentinel_.next->prev = sentinel_.prev;
			b.sentinel_.prev->next = &sentinel_;
			
			sentinel_.prev->next = b.sentinel_.next;
			sentinel_.prev = b.sentinel_.prev;
			
			b.list_common::unlink_all();
		}
	}
	
	void splice_front(list& b)
	{
		if(!b.empty())
		{
			b.sentinel_.prev->next = sentinel_.next;
			b.sentinel_.next->prev = &sentinel_;
			
			sentinel_.next->prev = b.sentinel_.prev;
			sentinel_.next = b.sentinel_.next;
			
			b.list_common::unlink_all();
		}
	}
	
	void split(iterator i, list& b)
	{
		if(i.ptr_ == &sentinel_)
			return; // Nothing to do
			
		sassert(sentinel_.prev != &sentinel_);

		gvl_list_node* new_last = i.ptr_->prev;
		i.ptr_->prev = b.sentinel_.prev;
		b.sentinel_.prev->next = i.ptr_;
		b.sentinel_.prev = sentinel_.prev;
		
		sentinel_.prev = new_last;
		new_last->next = &sentinel_;
	}

/*
	template<class Op>
	void sort(Op op)
	{
		list_common::sort(&op, do_list_compare<T, Op>);
	}*/
	
	template<typename Op>
	void merge(list& b, Op op)
	{
		forward_decycle_();
		b.forward_decycle_();
		sentinel_.next = _merge(sentinel_.next, b.sentinel_.next, op);
		stitch_up_();
		b.unlink_all();
	}
	
	template<typename Op>
	void sort(Op op)
	{
		if(empty())
			return;
			
		std::size_t const MaxBins = 25;
		gvl_list_node* binlist[MaxBins] = {};
		
		forward_decycle_();
		gvl_list_node* el = list_common::first();
				
		std::size_t max_bin = 0;
		
		while(el)
		{
			// Splice into temp and move el to the next element
			gvl_list_node* temp = el;
			el = el->next;
			temp->next = 0;
			
			std::size_t bin = 0;
			for(; bin < max_bin && binlist[bin]; ++bin)
			{
				temp = _merge(binlist[bin], temp, op);
				binlist[bin] = 0;
			}
			
			if(bin == MaxBins)
			{
				binlist[bin - 1] = _merge(binlist[bin - 1], temp, op);
			}
			else
			{
				binlist[bin] = temp;
				if(bin == max_bin)
					++max_bin;
			}
		}
		
		for(std::size_t bin = 1; bin < max_bin; ++bin)
		{
			binlist[bin] = _merge(binlist[bin], binlist[bin - 1], op);
		}
		
		sentinel_.next = binlist[max_bin - 1];
		
		stitch_up_();
	}
	
	
	/* Cross events are a too specific functionality
	struct dummy_cross_event
	{
		void operator()(T const&, T const&) const
		{
			// Do nothing
		}
	};*/
	
	template<typename Op/*, typename CrossEvent*/>
	void insertion_sort(Op op/*, CrossEvent cross_event = dummy_cross_event()*/)
	{
		gvl_list_node* sent = &sentinel_;
		gvl_list_node* lprev = sent;
		gvl_list_node* cur = lprev->next;
		
		while(cur != sent)
		{
			gvl_list_node* lnext = cur->next;
			T const* curt = downcast(cur);
			if(lprev != sent
			&& op(*curt, *downcast(lprev)))
			{
				//cross_event(*downcast(prev), *downcast(cur));
				
				// Unlink
				lprev->next = lnext;
				lnext->prev = lprev;
				
				gvl_list_node* before = lprev->prev;
				gvl_list_node* after = lprev;
				
				while(before != sent
				&& op(*curt, *downcast(before)))
				{
					//cross_event(*downcast(before), *downcast(cur));
					after = before;
					before = after->prev;
				}

				cur->next = after;
				cur->prev = before;
				
				before->next = cur;
				after->prev = cur;
				
				// prev stays the same here
			}
			else
				lprev = cur; // No move, prev will be cur
			
			cur = lnext;
		}
	}
	
protected:
	
	template<typename Op>
	gvl_list_node* _merge(gvl_list_node* first, gvl_list_node* second, Op& op)
	{
		if(!first)
			return second;
		else if(!second)
			return first;
			
		gvl_list_node* a = first;
		gvl_list_node* b = second;
		gvl_list_node* ret;
		gvl_list_node* prev;
		
		if(op(*downcast(b), *downcast(a)))
		{
			ret = b;
			prev = b;
			b = b->next;
		}
		else
		{
			ret = a;
			prev = a;
			a = a->next;
		}
			
		while(a && b)
		{
			if(op(*downcast(b), *downcast(a)))
			{
				prev->next = b;
				prev = b;
				b = b->next;
			}
			else
			{
				prev->next = a;
				prev = a;
				a = a->next;
			}
		}
		
		// Check if there's left-overs in any of the lists
		if(a)
			prev->next = a;
		else if(b)
			prev->next = b;
		
		return ret;
	}
	
	void forward_decycle_()
	{
		list_common::last()->next = 0; // Decycle chain
	}
	
	void stitch_up_()
	{
		// Fix back pointers and stitch up
		gvl_list_node* prev = &sentinel_;
		gvl_list_node* cur = sentinel_.next;
		for(; cur; cur = cur->next)
		{
			cur->prev = prev;
			prev = cur;
		}
		
		prev->next = &sentinel_;
		sentinel_.prev = prev;
	}
	
private:
	// Non-copyable
	list(list const& b);
	list& operator=(list const& b);
};


template<typename T, typename Tag = default_list_tag>
struct weak_list : list<T, Tag, dummy_delete>
{
	weak_list()
	{
	}
};

} // namespace gvl

#endif // UUID_D5923885C48B49588EAD94A276B131F0
