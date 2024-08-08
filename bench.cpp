#include "exactObjectList.hpp"
#include "math.hpp"
#include <gvl/math/cmwc.hpp>
#include <gvl/system/system.hpp>
#include <vector>
#include <memory>

#include <intrin.h>
#include <xmmintrin.h>

template<typename T, int Limit>
struct FixedObjectList
{
	struct range
	{
		range(T* cur, T* end)
		: cur(cur), end(end)
		{
		}

		T* next()
		{
			T* ret = cur;
			++cur;

			return ret == end ? 0 : ret;
		}

		T* cur;
		T* end;
	};

	FixedObjectList()
	{
		clear();
	}

	T* getFreeObject()
	{
		assert(count < Limit);
		T* ptr = &arr[count++];
		return ptr;
	}

	T* newObjectReuse()
	{
		T* ret;
		if(count == Limit)
			ret = &arr[Limit - 1];
		else
			ret = getFreeObject();

		return ret;
	}

	T* newObject()
	{
		if(count == Limit)
			return 0;

		T* ret = getFreeObject();
		return ret;
	}

	range all()
	{
		return range(arr, arr + count);
	}

	void free(T* ptr)
	{
		assert(ptr < &arr[0] + count && ptr >= &arr[0]);
		*ptr = arr[--count];
	}

	void free(range& r)
	{
		free(--r.cur);
		--r.end;
	}

	void clear()
	{
		count = 0;
	}

	std::size_t size() const
	{
		return count;
	}

	T arr[Limit];
	std::size_t count;
};

template<typename T, int Limit>
struct FixedCopyObjectList
{
	struct range
	{
		range(T* cur, T* end)
		: cur(cur), end(end)
		{
		}

		bool next(T*& ret)
		{
			ret = cur;
			++cur;

			return ret != end;
		}

		T* cur;
		T* end;
	};

	FixedCopyObjectList()
	{
		clear();
	}

	T* getFreeObject()
	{
		assert(count < Limit);
		T* ptr = &arr[count++];
		return ptr;
	}

	T* newObjectReuse()
	{
		T* ret;
		if(count == Limit)
			ret = &arr[Limit - 1];
		else
			ret = getFreeObject();

		return ret;
	}

	range all()
	{
		return range(arr, arr + count);
	}

	void clear()
	{
		count = 0;
	}

	std::size_t size() const
	{
		return count;
	}

	T arr[Limit];
	std::size_t count;
};

template<typename T, int Limit>
struct CompressObjectList
{
	struct range
	{
		range(T* cur, T* end)
		: cur(cur), end(end)
		{
		}

		T* next()
		{
			while (true)
			{
				if (cur == end)
					return 0;
				if (cur->used)
					break;
				++cur;
			}

			T* ret = cur;
			++cur;

			return ret;
		}

		T* cur;
		T* end;
	};

	CompressObjectList()
	{
		clear();
	}

	T* getFreeObject()
	{
		assert(next < Limit*2);
		T* ptr = &arr[next++];
		++count;
		ptr->used = true;
		return ptr;
	}

	T* newObjectReuse()
	{
		T* ret;
		if (next == Limit*2)
			ret = &arr[Limit*2 - 1];
		else
			ret = getFreeObject();

		return ret;
	}

	T* newObject()
	{
		if (next == Limit*2)
			return 0;

		T* ret = getFreeObject();
		return ret;
	}

	range all()
	{
		if (next > count * 2)
			compact();
		return range(arr, arr + next);
	}

	void free(T* ptr)
	{
		assert(ptr < &arr[0] + next && ptr >= &arr[0] && ptr->used);
		ptr->used = false;
		--count;
	}

	void free(range& r)
	{
		free(r.cur - 1);
	}

	void clear()
	{
		count = 0;
		next = 0;
	}

	std::size_t checkCount()
	{
		std::size_t c = 0;
		for (std::size_t i = 0; i < next; ++i)
		{
			c += arr[i].used ? 1 : 0;
		}

		return c;
	}

	void compact()
	{
		std::size_t to = 0;

		assert(checkCount() == count);

		for (; to < next && arr[to].used; ++to)
			/* Nothing */;

		std::size_t from = to;

		while (true)
		{
			while (from < next && !arr[from].used)
				++from;

			if (from >= next)
				break;

			arr[to++] = arr[from++];
		}

		next = to;

		assert(checkCount() == count);
	}

	std::size_t size() const
	{
		return count;
	}

	T arr[Limit*2];
	std::size_t count, next;
};

typedef int32_t loffset;

template<typename T, int Limit>
struct LinkedObjectList
{
	struct range
	{
		range(T* first, T* end)
		: cur(first), end(end)
		{
		}

		bool next(T*& ret)
		{
			ret = cur;
			cur = ret->next;

			return ret != end;
		}

		T* cur;
		T* end;
	};

	LinkedObjectList()
	{
		clear();
	}

	T* getFreeObject()
	{
		assert(count < Limit);
		T* ptr = freeList;
		freeList = freeList->next;

		ptr->prev = sentinel.prev;
		ptr->next = &sentinel;
		sentinel.prev->next = ptr;
		sentinel.prev = ptr;
		++count;
		return ptr;
	}

	T* newObjectReuse()
	{
		T* ret;
		if(!freeList)
			ret = &arr[Limit - 1];
		else
			ret = getFreeObject();

		return ret;
	}

	T* newObject()
	{
		if(count == Limit)
			return 0;

		T* ret = getFreeObject();
		return ret;
	}

	range all()
	{
		return range(sentinel.next, &sentinel);
	}

	void free(T* ptr)
	{
		auto p = ptr->prev, n = ptr->next;
		p->next = n;
		n->prev = p;
		ptr->next = freeList;
		freeList = ptr;
		--count;
	}

	void free(range& r)
	{
		free(r.cur->prev);
	}

	void clear()
	{
		count = 0;
		sentinel.prev = &sentinel;
		sentinel.next = &sentinel;

		freeList = 0;
		for (std::size_t i = Limit; i-- > 0; )
		{
			arr[i].next = freeList;
			freeList = &arr[i];
		}
	}

	std::size_t size() const
	{
		return count;
	}

	T arr[Limit];
	T* freeList;
	T sentinel;
	std::size_t count;
};

#define SINGLE (0)

template<typename T, int Limit>
struct LinkedObjectList2
{
	struct range
	{
		range(LinkedObjectList2* parent)
		: parent(parent), cur(&parent->sentinel)
		{
		}

		bool next(T*& ret)
		{
#if !SINGLE
			cur = parent->go(cur->next);
			ret = cur;
			return cur != &parent->sentinel;
#else
			prev = cur;
			cur = parent->go(prev->next);

			ret = cur;
			return cur != &parent->sentinel;
#endif
		}

		LinkedObjectList2* parent;
#if SINGLE
		T* prev;
#endif
		T* cur;
	};

	LinkedObjectList2()
	{
		clear();
	}

	T* go(loffset offs)
	{
		return (T*)((char*)this + offs);
	}

	loffset offs(T* next)
	{
		return (loffset)((char*)next - (char*)this);
	}

	T* getFreeObject()
	{
		assert(count < Limit);
		auto ptroffs = freeList;
		T* ptr = go(ptroffs);
		freeList = ptr->next;

#if !SINGLE
		ptr->prev = offs(&sentinel);
		ptr->next = sentinel.next;
		go(sentinel.next)->prev = ptroffs;
		sentinel.next = ptroffs;
#else
		ptr->next = sentinel.next;
		sentinel.next = ptroffs;
#endif
		++count;
		return ptr;
	}

	T* newObjectReuse()
	{
		T* ret;
		if(!freeList)
			ret = &arr[Limit - 1];
		else
			ret = getFreeObject();

		return ret;
	}

	T* newObject()
	{
		if(count == Limit)
			return 0;

		T* ret = getFreeObject();
		return ret;
	}

	range all()
	{
		return range(this);
	}

#if !SINGLE
	void free(T* ptr)
	{
		auto p = ptr->prev, n = ptr->next;
		go(p)->next = n;
		go(n)->prev = p;
		ptr->next = freeList;
		freeList = offs(ptr);
		--count;
	}
#endif

	void free(range& r)
	{
#if !SINGLE
		T* c = r.cur;
		r.cur = go(r.cur->prev);
		free(c);
#else
		auto prev = r.prev;
		prev->next = r.cur->next;

		r.cur->next = freeList;
		freeList = offs(r.cur);

		r.cur = prev;
		--count;
#endif
	}

	void clear()
	{
		count = 0;
#if !SINGLE
		sentinel.prev = offs(&sentinel);
#endif
		sentinel.next = offs(&sentinel);

		freeList = 0;
		for (std::size_t i = Limit; i-- > 0; )
		{
			arr[i].next = freeList;
			freeList = offs(&arr[i]);
		}
	}

	std::size_t size() const
	{
		return count;
	}

	T sentinel;
	T arr[Limit];
	loffset freeList;
	std::size_t count;
};


template<typename T, int Limit>
struct LinkedObjectList3
{
	struct range
	{
		range(T* first, T* end)
		: cur(first), end(end)
		{
		}

		bool next(T*& ret)
		{
			ret = cur;
			++cur;

			return ret != end;
		}

		T* cur;
		T* end;
	};

	LinkedObjectList3()
	{
		clear();
	}

	T* go(loffset offs)
	{
		return (T*)((char*)this + offs);
	}

	loffset offs(T* next)
	{
		return (loffset)((char*)next - (char*)this);
	}

	T* getFreeObject()
	{
		assert(count < Limit);

		T* ptr = arr + count;
		auto ptroffs = offs(ptr);
		ptr->prev = offs(&sentinel);
		ptr->next = sentinel.next;
		go(sentinel.next)->prev = ptroffs;
		sentinel.next = ptroffs;

		++count;
		return ptr;
	}

	T* newObjectReuse()
	{
		T* ret;
		if (count == Limit)
			ret = &arr[Limit - 1];
		else
			ret = getFreeObject();

		return ret;
	}

	T* newObject()
	{
		if(count == Limit)
			return 0;

		T* ret = getFreeObject();
		return ret;
	}

	range all()
	{
		return range(arr, arr + count);
	}

	void unlink(T* ptr)
	{
		auto p = ptr->prev, n = ptr->next;
		go(p)->next = n;
		go(n)->prev = p;
	}

	void free(T* ptr)
	{
		assert(ptr < &arr[0] + count && ptr >= &arr[0]);

		T* last = &arr[--count];

		unlink(ptr);

		{
			auto ptroffset = offs(ptr);
			auto p = last->prev, n = last->next;
			go(p)->next = ptroffset;
			go(n)->prev = ptroffset;
		}

		*ptr = *last;
	}

	void free(range& r)
	{
		free(--r.cur);
		--r.end;
	}

	void clear()
	{
		count = 0;
#if !SINGLE
		sentinel.prev = offs(&sentinel);
#endif
		sentinel.next = offs(&sentinel);
	}

	std::size_t size() const
	{
		return count;
	}

	T sentinel;
	T arr[Limit];
	std::size_t count;
};

template<typename Container, typename T>
struct Bruteforce
{
	typedef typename Container::range area_range;

	void add(T* ptr)
	{
	}

	void move(T* ptr)
	{
	}

	void remove(T* ptr)
	{
	}

	area_range area(Container& container, fixed x1, fixed y1, fixed x2, fixed y2)
	{
		return container.all();
	}
};

struct LinkedCellObjectBase
{
	LinkedCellObjectBase()
	: index(0xffffffff)
	{
	}

	LinkedCellObjectBase *cnext, *cprev;
	uint32_t index;
};

template<typename Container, typename T>
struct Cellphase
{
	struct area_range
	{
		LinkedCellObjectBase* base;
		LinkedCellObjectBase* n;
		LinkedCellObjectBase* end;
		int xbeg;
		int x, y;
		int xend, yend;

		bool next(T*& ret)
		{
			while (true)
			{
				n = n->cnext;
				if (n != end)
				{
					ret = static_cast<T*>(n);
					return true;
				}

				if (x == xend)
				{
					if (y == yend)
						return false;
					y += gridWidth;
					x = xbeg;
				}

				++x;

				n = &base[(x + y) & (cellMask | (cellMask << gridShift))];
				end = n;
			}
		}
	};

	uint32_t cell(int x, int y)
	{
		uint32_t cx = clamp(x >> cellShift);
		uint32_t cy = clamp(y >> cellShift);
		return cy * gridWidth + cx;
	}

	void add(T* ptr)
	{
		uint32_t cx = (ptr->pos.x >> (16 + cellShift));
		uint32_t cy = (ptr->pos.y >> (16 + cellShift)) << gridShift;
		auto newIndex = (cx + cy) & (cellMask | (cellMask << gridShift));
		auto* s = cells + newIndex;
		auto* sp = s->cprev;

		sp->cnext = ptr;
		ptr->cnext = s;
		ptr->cprev = sp;
		s->cprev = ptr;
		ptr->index = newIndex;
	}

	void move(T* ptr)
	{
		uint32_t cx = (ptr->pos.x >> (16 + cellShift));
		uint32_t cy = (ptr->pos.y >> (16 + cellShift)) << gridShift;
		auto newIndex = (cx + cy) & (cellMask | (cellMask << gridShift));

		if (newIndex != ptr->index)
		{
			remove(ptr);

			auto* s = cells + newIndex;
			auto* sp = s->cprev;
			sp->cnext = ptr;
			ptr->cnext = s;
			ptr->cprev = sp;
			s->cprev = ptr;
			ptr->index = newIndex;
		}
	}

	void remove(T* ptr)
	{
		auto n = ptr->cnext, p = ptr->cprev;
		p->cnext = n;
		n->cprev = p;
	}

	uint32_t clamp(int v)
	{
		return (uint32_t)v & cellMask;
	}

	area_range area(Container& container, fixed x1, fixed y1, fixed x2, fixed y2)
	{
		area_range ar;

		int cx1 = (x1 >> (16 + cellShift));
		int cy1 = (y1 >> (16 + cellShift)) << gridShift;
		int cx2 = (x2 >> (16 + cellShift));
		int cy2 = (y2 >> (16 + cellShift)) << gridShift;

		LinkedCellObjectBase* first = &cells[(cx1 + cy1) & (cellMask | (cellMask << gridShift))];

		ar.n = first;
		ar.end = first;
		ar.base = cells;
		ar.x = cx1;
		ar.xbeg = cx1 - 1;
		ar.y = cy1;
		ar.xend = cx2;
		ar.yend = cy2;
		return ar;
	}

	Cellphase()
	{
		for (std::size_t i = 0; i < gridWidth * gridWidth; ++i)
		{
			cells[i].cnext = &cells[i];
			cells[i].cprev = &cells[i];
		}
	}

	static uint32_t const cellShift = 1;
	static uint32_t const worldShift = 9; // 512
	static uint32_t const gridShift = (worldShift - cellShift);
	static uint32_t const gridWidth = (uint32_t(1) << gridShift);
	static uint32_t const cellMask = gridWidth - 1;

	LinkedCellObjectBase cells[gridWidth * gridWidth];
};

struct Object
{
	fixedvec pos, vel;
	int timeLeft;
	int type;

	bool process()
	{
		pos += vel;

		if (pos.x >= itof(512))
		{
			pos.x = itof(512) - 1;
			vel.x = -std::abs(vel.x);
		}
		else if (pos.x < 0)
		{
			pos.x = 0;
			vel.x = std::abs(vel.x);
		}

		if (pos.y >= itof(512))
		{
			pos.y = itof(512) - 1;
			vel.y = -std::abs(vel.y);
		}
		else if (pos.y < 0)
		{
			pos.y = 0;
			vel.y = std::abs(vel.y);
		}

		if (--timeLeft < 0)
			return false;

		return true;
	}

	template<typename Container>
	static void process2(Object& obj, Container* cont)
	{
		auto vel = obj.vel;
		auto pos = obj.pos + vel;
		auto timeLeft = obj.timeLeft;

		if (pos.x >= itof(512))
		{
			pos.x = itof(512) - 1;
			vel.x = -std::abs(vel.x);
		}
		else if (pos.x < 0)
		{
			pos.x = 0;
			vel.x = std::abs(vel.x);
		}

		if (pos.y >= itof(512))
		{
			pos.y = itof(512) - 1;
			vel.y = -std::abs(vel.y);
		}
		else if (pos.y < 0)
		{
			pos.y = 0;
			vel.y = std::abs(vel.y);
		}

		if (--timeLeft >= 0)
		{
			auto type = obj.type;

			Object& obj = *cont->newObjectReuse();
			obj.pos = pos;
			obj.vel = vel;
			obj.timeLeft = timeLeft;
			obj.type = type;
		}
	}
};

struct ObjectExact : ExactObjectListBase, Object
{
};

template<typename T>
struct LinkedObjectListBase
{
	T* next;
#if !SINGLE
	T* prev;
#endif
};

struct ObjectLinked : LinkedObjectListBase<ObjectLinked>, Object
{
};

struct LinkedObjectListBase2
{
	loffset next, prev;
};

struct ObjectLinked2 : LinkedObjectListBase2, Object
{

};

struct ObjectLinkedC : LinkedCellObjectBase, LinkedObjectListBase2, Object
{

};

#define BROADPHASE (1)

template<typename E, typename Container, typename Broadphase>
struct State
{
	gvl::default_xorshift rand;
	std::unique_ptr<Container> container;
	std::unique_ptr<Container> container2;

#if BROADPHASE
	Broadphase broadphase;
#endif

	fixedvec pos;

	State()
	: rand(1)
	, container(new Container)
	{
	}

	void bench2()
	{
		int frames = 0;

		container2.reset(new Container);

		auto* cont = container.get();
		auto* cont2 = container2.get();

		for (int i = 0; i < 100000; ++i)
		{
			// Create 5
			pos.x = rand(itof(512 - 20)) + itof(10);
			pos.y = rand(itof(512 - 20)) + itof(10);

			int maxlife = cont->count < 800 ? 40 : 8;
			int create = cont->count < 800 ? 100 : 4;
			for (int c = 0; c < create; ++c)
			{
				E* obj = cont->newObjectReuse();
				obj->pos = pos;
				obj->pos.x += rand(itof(40)) - itof(20);
				obj->pos.y += rand(itof(40)) - itof(20);
				obj->vel.x = rand(300) - 150;
				obj->vel.y = rand(300) - 150;
				obj->timeLeft = rand(maxlife);
				obj->type = rand(2);
			}

			for (int c = 0; c < 5; ++c)
			{
				++frames;
				auto r = cont->all();
				for (E* i; r.next(i); )
				{
					E::process2(*i, cont2);
				}
			}

			std::swap(cont, cont2);
			cont2->clear();
		}
	}

	void bench()
	{
		int collisions = 0;
		int frames = 0;
		int tests = 0;

		auto& cont = *container;

		for (int i = 0; i < 100000; ++i)
		//for (int i = 0; i < 2000; ++i)
		{
			// Create 5
			pos.x = rand(itof(512 - 20)) + itof(10);
			pos.y = rand(itof(512 - 20)) + itof(10);

			int maxlife = cont.count < 800 ? 40 : 8;
			int create = cont.count < 800 ? 100 : 4;
			for (int c = 0; c < create; ++c)
			{
				E* obj = cont.newObjectReuse();
				obj->pos = pos;
				obj->pos.x += rand(itof(40)) - itof(20);
				obj->pos.y += rand(itof(40)) - itof(20);
				obj->vel.x = rand(300) - 150;
				obj->vel.y = rand(300) - 150;
				obj->timeLeft = rand(maxlife);
				obj->type = rand(2);
#if BROADPHASE
				broadphase.add(obj);
#endif
			}

			for (int c = 0; c < 5; ++c)
			{
				++frames;
				auto r = cont.all();
				for (E* i; r.next(i); )
				{
					if (!i->process())
					{
#if BROADPHASE
						broadphase.remove(i);
#endif
						cont.free(r);
					}
					else
					{
#if BROADPHASE
						broadphase.move(i);
#endif

#if 0
						if (i->type == 0)
						{
							auto pos = i->pos;
							int itype = i->type;
							auto impulse = i->vel / 4;
							auto ar = broadphase.area(cont, pos.x - itof(2), pos.y - itof(2), pos.x + itof(2), pos.y + itof(2));
							for (E* o; ar.next(o); )
							{
								++tests;
								if (o->type != 0
								 && (uint32_t)(pos.x - o->pos.x + itof(2)) <= (uint32_t)itof(4)
								 && (uint32_t)(pos.y - o->pos.y + itof(2)) <= (uint32_t)itof(4))
								{
									o->vel += impulse;
									++collisions;
								}
							}
						}
#endif
					}
				}
			}

			/*
			if ((i % 250) == 1)
			{
				printf("%d\n", container.count);
			}*/
		}

		printf("Collisions: %d, Tests: %d, Frames: %d, Per frame: %f\n", collisions, tests, frames, (double)collisions / frames);
	}
};

void benchAll()
{
	uint32_t t;

	/*
	t = gvl::get_ticks();
	State<ObjectLinked, LinkedObjectList<ObjectLinked, 1000>> state3;
	state3.bench();
	t = gvl::get_ticks() - t;
	printf("%f\n", t / 1000.0);
	*/

#if 1
	{
		t = gvl::get_ticks();
		auto* state = new State<ObjectLinked2, LinkedObjectList2<ObjectLinked2, 1000>, Bruteforce<LinkedObjectList2<ObjectLinked2, 1000>, ObjectLinked2>>;
		state->bench();
		t = gvl::get_ticks() - t;
		printf("%f\n", t / 1000.0);
	}
#endif

#if 1
	{
		t = gvl::get_ticks();
		auto* state = new State<ObjectLinked2, LinkedObjectList3<ObjectLinked2, 1000>, Bruteforce<LinkedObjectList3<ObjectLinked2, 1000>, ObjectLinked2>>;
		state->bench();
		t = gvl::get_ticks() - t;
		printf("%f\n", t / 1000.0);
	}
#endif

#if 0
	t = gvl::get_ticks();
	State<Object, FixedCopyObjectList<Object, 1000>, Bruteforce<FixedCopyObjectList<Object, 1000>, Object>> state3;
	state3.bench2();
	t = gvl::get_ticks() - t;
	printf("%f\n", t / 1000.0);
#endif

#if 0
	t = gvl::get_ticks();
	auto* state6 = new State<ObjectLinkedC, LinkedObjectList2<ObjectLinkedC, 1000>, Cellphase<LinkedObjectList2<ObjectLinkedC, 1000>, ObjectLinkedC>>;
	state6->bench();
	t = gvl::get_ticks() - t;
	printf("%f\n", t / 1000.0);
#endif

	/*
	t = gvl::get_ticks();
	State<ObjectExact, ExactObjectList<ObjectExact, 1000>> state1;
	state1.bench();
	t = gvl::get_ticks() - t;
	printf("%f\n", t / 1000.0);


	t = gvl::get_ticks();
	State<Object, FixedObjectList<Object, 1000>> state2;
	state2.bench();
	t = gvl::get_ticks() - t;
	printf("%f\n", t / 1000.0);
	*/





	/*
	t = gvl::get_ticks();
	State<ObjectExact, CompressObjectList<ObjectExact, 1000>> state4;
	state4.bench();
	t = gvl::get_ticks() - t;
	printf("%f\n", t / 1000.0);
	*/
}