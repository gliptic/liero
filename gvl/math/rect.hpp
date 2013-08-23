#ifndef UUID_63536246013C464DDB1F129027E54907
#define UUID_63536246013C464DDB1F129027E54907

#include "vec.hpp"

#include <algorithm> // For std::min, std::max

namespace gvl
{

template<typename T>
class basic_rect
{
public:
	basic_rect()
	: x1(0), y1(0), x2(0), y2(0)
	{ }

	basic_rect(T x1_, T y1_, T x2_, T y2_)
	: x1(x1_), y1(y1_), x2(x2_), y2(y2_)
	{ }
	
	basic_rect(basic_vec<T, 2> const& pixel)
	: x1(pixel.x), y1(pixel.y)
	, x2(pixel.x + T(1)), y2(pixel.y + T(1))
	{ }
	
	basic_rect(basic_vec<T, 2> const& center, T size)
	: x1(center.x - size), y1(center.y - size)
	, x2(center.x + size), y2(center.y + size)
	{ }
	
	basic_rect(basic_vec<T, 2> const& center, T w, T h)
	: x1(center.x - w/2), y1(center.y - h/2)
	, x2(center.x + w/2), y2(center.y + h/2)
	{ }
	
	basic_rect(basic_vec<T, 2> const& corner1, basic_vec<T, 2> const& corner2)
	{
		if(corner1.x < corner2.x)
		{
			x1 = corner1.x;
			x2 = corner2.x + 1;
		}
		else
		{
			x1 = corner2.x;
			x2 = corner1.x + 1;
		}
		
		if(corner1.y < corner2.y)
		{
			y1 = corner1.y;
			y2 = corner2.y + 1;
		}
		else
		{
			y1 = corner2.y;
			y2 = corner1.y + 1;
		}
	}
	
	T x1;
	T y1;
	T x2;
	T y2;
	
	T center_x() const
	{ return (x1 + x2) / T(2); }
	
	T center_y() const
	{ return (y1 + y2) / T(2); }
	
	basic_vec<T, 2> center() const
	{ return basic_vec<T, 2>(center_x(), center_y()); }

	T width() const
	{ return x2 - x1; }

	T height() const
	{ return y2 - y1; }
	
	basic_rect flip() const
	{ return basic_rect<T>(y1, x1, y2, x2); }

	bool valid()
	{ return x1 <= x2 && y1 <= y2; }
	
	void join(basic_rect const& b)
	{
		x1 = std::min(b.x1, x1);
		y1 = std::min(b.y1, y1);
		x2 = std::max(b.x2, x2);
		y2 = std::max(b.y2, y2);
	}
		
	bool precise_join(basic_rect const& b)
	{
		bool ok = false;
		if(x1 == b.x1 && x2 == b.x2)
		{
			if(b.y2 >= y1
			&& b.y1 <= y2)
				ok = true;
		}
		else if(y1 == b.y1 && y2 == b.y2)
		{
			if(b.x2 >= x1
			&& b.x1 <= x2)
				ok = true;
		}
		else
		{
			ok = inside(b) || b.inside(*this);
		}
		
		if(ok)
			join(b);
		return ok;
	}
	
	bool inside(basic_rect const& b) const
	{
		return x1 <= b.x1 && x2 >= b.x2
		    && y1 <= b.y1 && y2 >= b.y2;
	}
	
	bool join_h(basic_rect const& b)
	{
		T new_x1 = std::min(b.x1, x1);
		T new_x2 = std::max(b.x2, x2);
		
		bool changed = new_x1 != x1 || new_x2 != x2;
		x1 = new_x1;
		x2 = new_x2;
		
		return changed;
	}
	
	bool join_v(basic_rect const& b)
	{
		T new_y1 = std::min(b.y1, y1);
		T new_y2 = std::max(b.y2, y2);
		
		bool changed = new_y1 != y1 || new_y2 != y2;
		y1 = new_y1;
		y2 = new_y2;
		
		return changed;
	}

	// Extend *this and b to their maximal size without
	// changing their joint coverage.
	int maximal_extend(basic_rect& b)
	{
		int change_mask = 0;
		if(intersecting_v(b))
		{
			if(encloses_h(b))
			{
				// Extend b vertically into *this
				if(b.join_v(*this))
					change_mask |= 2;
			}
			if(b.encloses_h(*this))
			{
				// Extend *this vertically into b
				if(join_v(b))
					change_mask |= 1;
			}
		}
		
		if(intersecting_h(b))
		{
			if(encloses_v(b))
			{
				// Extend b horizontally into *this
				if(b.join_h(*this))
					change_mask |= 2;
			}
			if(b.encloses_v(*this))
			{
				// Extend *this horizontally into b
				if(join_h(b))
					change_mask |= 1;
			}
		}
		
		return change_mask;
	}
	
	// Is b inside *this?
	bool encloses(basic_rect const& b) const
	{
		return encloses_h(b) && encloses_v(b);
	}
	
	// Is the horizontal span of b inside *this?
	bool encloses_h(basic_rect const& b) const
	{
		return x1 <= b.x1 && x2 >= b.x2;
	}
	
	// Is the vertical span of b inside *this?
	bool encloses_v(basic_rect const& b) const
	{
		return y1 <= b.y1 && y2 >= b.y2;
	}
	
	bool proper_intersecting(basic_rect const& b) const
	{
		return (b.y2 > y1
		     && b.y1 < y2
		     && b.x2 > x1
		     && b.x1 < x2);
	}
	
	bool intersecting_h(basic_rect const& b) const
	{
		return (b.x2 >= x1
		     && b.x1 <= x2);
	}
	
	bool intersecting_v(basic_rect const& b) const
	{
		return (b.y2 >= y1
		     && b.y1 <= y2);
	}

	// TODO: This isn't really intersecting!
	// Also returns true when the rectangles are merely touching.
	// What to do about that? Added proper_intersecting above for now.
	bool intersecting(basic_rect const& b) const
	{
		return intersecting_h(b) && intersecting_v(b);
	}

	bool intersect(basic_rect const& b)
	{
		x1 = std::max(b.x1, x1);
		y1 = std::max(b.y1, y1);
		x2 = std::min(b.x2, x2);
		y2 = std::min(b.y2, y2);
		
		return valid();
	}

	bool encloses(basic_vec<T, 2> v) const
	{ return encloses(v.x, v.y); }
	
	bool encloses(T x, T y) const
	{
		T diffX = x - x1;
		T diffY = y - y1;
		
		return diffX < width() && diffX >= T(0)
		    && diffY < height() && diffY >= T(0);
	}
	
	basic_rect operator&(basic_rect const& b) const
	{ basic_rect ret(*this); ret &= b; return ret; }
	
	basic_rect& operator&=(basic_rect const& b)
	{
		intersect(b);
		return *this;
	}
	
	basic_rect operator|(basic_rect const& b) const
	{ basic_rect ret(*this); ret |= b; return ret; }
	
	basic_rect& operator|=(basic_rect const& b)
	{
		join(b);
		return *this;
	}
	
	basic_rect operator+(basic_vec<T, 2> const& b)
	{ basic_rect ret(*this); ret += b; return ret; }
	
	basic_rect& operator+=(basic_vec<T, 2> const& b)
	{
		x1 += b.x; x2 += b.x;
		y1 += b.y; y2 += b.y;
		return *this;
	}
	
	basic_rect operator-(basic_vec<T, 2> const& b)
	{ basic_rect ret(*this); ret -= b; return ret; }
	
	basic_rect& operator-=(basic_vec<T, 2> const& b)
	{
		x1 -= b.x; x2 -= b.x;
		y1 -= b.y; y2 -= b.y;
		return *this;
	}
	
	basic_rect translated(T x, T y)
	{ return *this + basic_vec<T, 2>(x, y); }
	
	void translate_v(T y)
	{
		y1 += y; y2 += y;
	}
	
	void translate_h(T x)
	{
		x1 += x; x2 += x;
	}
	
	basic_vec<T, 2> ul()
	{ return basic_vec<T, 2>(x1, y1); }
	
	basic_vec<T, 2> ur()
	{ return basic_vec<T, 2>(x2, y1); }
	
	basic_vec<T, 2> ll()
	{ return basic_vec<T, 2>(x1, y2); }
	
	basic_vec<T, 2> lr()
	{ return basic_vec<T, 2>(x2, y2); }
	
	bool operator==(basic_rect const& b) const
	{ return x1 == b.x1 && y1 == b.y1 && x2 == b.x2 && y2 == b.y2; }
	
	bool operator!=(basic_rect const& b) const
	{ return !operator==(b); }
};

typedef basic_rect<int> rect;
typedef basic_rect<float> frect;

} // namespace gvl

#endif // UUID_63536246013C464DDB1F129027E54907
