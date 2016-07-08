#ifndef UUID_58A289E8AF6540DE3238628A4259BAAF
#define UUID_58A289E8AF6540DE3238628A4259BAAF

#include "platform.h"

typedef struct tl_list_node
{
	struct tl_list_node* next;
	struct tl_list_node* prev;
} tl_list_node;

typedef struct tl_list
{
	tl_list_node sentinel;
} tl_list;

TL_INLINE void tl_list_init(tl_list* self)
{
	self->sentinel.next = &self->sentinel;
	self->sentinel.prev = &self->sentinel;
}

TL_INLINE void tl_list_link_after(tl_list_node* self, tl_list_node* new_node)
{
	tl_list_node* old_self_next = self->next;

	new_node->next = old_self_next;
	new_node->prev = self;
	old_self_next->prev = new_node;
	self->next = new_node;
}

TL_INLINE void tl_list_link_before(tl_list_node* self, tl_list_node* new_node)
{
	tl_list_node* old_self_prev = self->prev;

	new_node->next = self;
	new_node->prev = old_self_prev;
	old_self_prev->next = new_node;
	self->prev = new_node;
}

TL_INLINE void tl_list_unlink(tl_list_node* self)
{
	tl_list_node* self_next = self->next;
	tl_list_node* self_prev = self->prev;

	self_prev->next = self_next;
	self_next->prev = self_prev;
}

#define tl_list_first(self) ((self)->sentinel.next)
#define tl_list_last (self) ((self)->sentinel.prev)

#define tl_list_add(self, node)    (tl_list_link_before(&(self)->sentinel, node))
#define tl_list_remove(self, node) (tl_list_unlink(node))

#define tl_list_getnode(t, sub, node) ((t*)((char*)(node) - offsetof(t,sub)))

#define tl_list_foreach(l, t, sub, var, body) do { \
	tl_list* fl_ = (l); \
	tl_list_node* fvar_ = tl_list_first(fl_); \
	for(; fvar_ != &fl_->sentinel; fvar_ = fvar_->next) { \
		t* var = tl_list_getnode(t, sub, fvar_); \
		body \
	} \
} while(0)

#endif // UUID_58A289E8AF6540DE3238628A4259BAAF
