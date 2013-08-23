#ifndef UUID_BFFA58059B0C49630DD657937FF53E6C
#define UUID_BFFA58059B0C49630DD657937FF53E6C

#include "../support/platform.hpp"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gvl_list_node_
{
	struct gvl_list_node_* next;
	struct gvl_list_node_* prev;
} gvl_list_node;

typedef struct gvl_list_
{
	gvl_list_node sentinel;
} gvl_list;

GVL_INLINE void gvl_list_link_after(gvl_list_node* self, gvl_list_node* new_node)
{
	gvl_list_node* old_self_next = self->next;
	
	new_node->next = old_self_next;
	new_node->prev = self;
	old_self_next->prev = new_node;
	self->next = new_node;
}

GVL_INLINE void gvl_list_link_before(gvl_list_node* self, gvl_list_node* new_node)
{
	gvl_list_node* old_self_prev = self->prev;
	
	new_node->next = self;
	new_node->prev = old_self_prev;
	old_self_prev->next = new_node;
	self->prev = new_node;
}

GVL_INLINE void gvl_list_unlink(gvl_list_node* self)
{
	gvl_list_node* self_next = self->next;
	gvl_list_node* self_prev = self->prev;
	
	self_prev->next = self_next;
	self_next->prev = self_prev;
}

#define gvl_list_first(self) ((self)->sentinel.next)
#define gvl_list_last (self) ((self)->sentinel.prev)

#ifdef __cplusplus
}
#endif

#endif // UUID_BFFA58059B0C49630DD657937FF53E6C
