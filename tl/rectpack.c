#include "rectpack.h"

#include <malloc.h>
#include <assert.h>

void tl_rectpack_init(tl_rectpack* self, tl_recti r)
{
	self->parent = NULL;
	self->largest_free_width = tl_rect_width(r);
	self->largest_free_height = tl_rect_height(r);
	self->enclosing = r;
	self->ch[0] = NULL;
	self->ch[1] = NULL;
	self->occupied = 0;
}

static int may_fit(tl_rectpack* self, int w, int h, int allow_rotate)
{
	return (w <= self->largest_free_width && h <= self->largest_free_height)
	    || (allow_rotate && h <= self->largest_free_width && w <= self->largest_free_height);
}

static int should_rotate(int cw, int ch, int w, int h)
{
	if(w > ch || h > cw)
		return 0; // Cannot rotate
	else if(h > ch || w > cw)
		return 1; // Must rotate

	// Try to counter-act the shape of the container (making the
	// uncovered parts more square).
	return (cw < ch) == (w < h);
}

static void propagate_largest(tl_rectpack* self, int w, int h);

static void propagate_largest_nonleaf(tl_rectpack* self)
{
	tl_rectpack* ch0 = self->ch[0];
	tl_rectpack* ch1 = self->ch[1];

	if(!ch1)
		propagate_largest(self, ch0->largest_free_width, ch0->largest_free_height);
	else if(!ch0)
		propagate_largest(self, ch1->largest_free_width, ch1->largest_free_height);
	else
	{
		int new_width = ch0->largest_free_width;
		int new_height = ch0->largest_free_height;
		if(ch1->largest_free_width > new_width)
			new_width = ch1->largest_free_width;
		if(ch1->largest_free_height > new_height)
			new_height = ch1->largest_free_height;

		propagate_largest(self, new_width, new_height);
	}
}

static void propagate_largest(tl_rectpack* self, int w, int h)
{
	self->largest_free_width = w;
	self->largest_free_height = h;
	if(self->parent)
		propagate_largest_nonleaf(self->parent);
}

static void propagate_largest_(tl_rectpack* self)
{
	if(!self->ch[0] && !self->ch[1])
	{
		if(self->occupied) propagate_largest(self, 0, 0);
		else propagate_largest(self, tl_rect_width(self->base.rect), tl_rect_height(self->base.rect));
	}
	else
	{
		propagate_largest_nonleaf(self);
	}
}

static tl_rectpack* rectpack_create(tl_rectpack* parent, int x1, int y1, int x2, int y2)
{
	tl_recti r;
	tl_rectpack* self = malloc(sizeof(tl_rectpack));
	r.x1 = x1;
	r.y1 = y1;
	r.x2 = x2;
	r.y2 = y2;
	tl_rectpack_init(self, r);
	self->parent = parent;
	return self;
}

static tl_packed_rect* known_fit(tl_rectpack* self, int w, int h)
{
	assert(w <= self->largest_free_width && h <= self->largest_free_height);
	assert(!self->occupied);
	assert(self->largest_free_width == tl_rect_width(self->enclosing));
	assert(self->largest_free_height == tl_rect_height(self->enclosing));

	self->occupied = 1;

	self->base.rect.x1 = self->enclosing.x1;
	self->base.rect.y1 = self->enclosing.y1;
	self->base.rect.x2 = self->enclosing.x1 + w;
	self->base.rect.y2 = self->enclosing.y1 + h;

	if(w == self->largest_free_width && h == self->largest_free_height)
	{
		propagate_largest(self, 0, 0);
		return (tl_packed_rect*)self;
	}

	{
		int enc_w = self->largest_free_width;
		int enc_h = self->largest_free_height;
		int space_w = (enc_w - w);
		int space_h = (enc_h - h);

		int cut_horiz = space_w < space_h;

		if(cut_horiz)
		{
			if(w < enc_w)
				self->ch[0] = rectpack_create(self, self->enclosing.x1 + w, self->enclosing.y1, self->enclosing.x2, self->enclosing.y1 + h);
			self->ch[1] = rectpack_create(self, self->enclosing.x1, self->enclosing.y1 + h, self->enclosing.x2, self->enclosing.y2);
		}
		else
		{
			if(h < enc_h)
				self->ch[0] = rectpack_create(self, self->enclosing.x1, self->enclosing.y1 + h, self->enclosing.x1 + w, self->enclosing.y2);
			self->ch[1] = rectpack_create(self, self->enclosing.x1 + w, self->enclosing.y1, self->enclosing.x2, self->enclosing.y2);
		}

		propagate_largest_nonleaf(self);
	}

	return (tl_packed_rect*)self;
}

tl_packed_rect* tl_rectpack_try_fit(tl_rectpack* self, int w, int h, int allow_rotate)
{
	tl_rectpack* ch0 = self->ch[0];
	tl_rectpack* ch1 = self->ch[1];
	if(!may_fit(self, w, h, allow_rotate))
		return NULL;

	if(ch1)
	{
		if(ch0)
		{
			tl_packed_rect* r = tl_rectpack_try_fit(ch0, w, h, allow_rotate);
			if(r) return r;
		}
		return tl_rectpack_try_fit(ch1, w, h, allow_rotate);
	}

	if(self->occupied)
		return NULL;

	if(allow_rotate && should_rotate(self->largest_free_width, self->largest_free_height, w, h))
		return known_fit(self, h, w);
	return known_fit(self, w, h);
}

void tl_rectpack_remove(tl_rectpack* self, tl_packed_rect* r)
{
	assert(!"unimplemented");
}

void tl_rectpack_clear(tl_rectpack* self)
{
	if(self->ch[0]) { tl_rectpack_deinit(self->ch[0]); free(self->ch[0]); self->ch[0] = NULL; }
	if(self->ch[1]) { tl_rectpack_deinit(self->ch[1]); free(self->ch[1]); self->ch[1] = NULL; }
	// propagate largest
	self->occupied = 0;
	propagate_largest_(self);
}

void tl_rectpack_deinit(tl_rectpack* self)
{
	tl_rectpack_clear(self);
}
