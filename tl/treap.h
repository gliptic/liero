#ifndef UUID_A150C7AB6941491F46D3788208FD1F48
#define UUID_A150C7AB6941491F46D3788208FD1F48

#include "config.h"
#include "cstdint.h"
#include <stddef.h>

typedef struct tl_treap_node {
	struct tl_treap_node* ch[2];
	struct tl_treap_node* parent;
	uint32_t prio;
} tl_treap_node;

extern tl_treap_node tl_treap_null;

typedef struct treap {
	tl_treap_node header;
	uint32_t x, y, z, w;
} treap;

TL_TREAP_API void tl_treap_init_node(treap* t, tl_treap_node* el);
TL_TREAP_API void tl_treap_init(treap* t);
TL_TREAP_API void tl_treap_remove(treap* t, tl_treap_node* el);

TL_TREAP_API void tl_treap_restore_heap(tl_treap_node* p, tl_treap_node* el, int dir);
TL_TREAP_API tl_treap_node* tl_treap_leftmost(tl_treap_node* p);
TL_TREAP_API void tl_treap_clear(tl_treap_node* p, int offset);
TL_TREAP_API uint32_t tl_treap_genrand_int32(treap* self);

#define tl_treap_remove_g(t, sub, el) tl_treap_remove(t, &(el)->sub)

#define tl_treap_insert_g(t, type,sub, el, gt,eq) do { \
	treap *t_ = (t); \
	type *node = (el), *other; \
	tl_treap_node *el_ = &node->sub, *p_ = &t_->header; \
	int dir_ = 0; \
	tl_treap_init_node(t_, el_); \
	do { \
		if(p_->ch[dir_] == &tl_treap_null) { \
			(el) = NULL; \
			tl_treap_restore_heap(p_, el_, dir_); \
			break; \
		} \
		other = tl_treap_getnode(type,sub, (p_ = p_->ch[dir_])); \
		dir_ = (gt); \
	} while(dir_ || !(eq)); \
} while(0)

#define tl_treap_insert_dup_g(t, type,sub, el, gt) do { \
	treap *t_ = (t); \
	type *node = (el), *other; \
	tl_treap_node *el_ = &node->sub, *p_ = &t_->header; \
	int dir_ = 0; \
	tl_treap_init_node(t_, el_); \
	while(p_->ch[dir_] != &tl_treap_null) { \
		other = tl_treap_getnode(type,sub, (p_ = p_->ch[dir_])); \
		dir_ = (gt); \
	} \
	tl_treap_restore_heap(p_, el_, dir_); \
} while(0)

#define tl_treap_getnode(type,sub, node) ((type*)((char*)(node) - offsetof(type,sub)))

#define tl_treap_largest_where_g(r, t, type,sub, pred) do { \
	tl_treap_node *p_ = &(t)->header, *rightmost_ = NULL; \
	int dir_ = 0; \
	while(p_->ch[dir_] != &tl_treap_null) { \
		type* other = tl_treap_getnode(type,sub, (p_ = p_->ch[dir_])); \
		dir_ = (pred); \
		if(dir_ == 1) rightmost_ = p_; \
	} \
	r = tl_treap_getnode(type,sub, rightmost_); \
} while(0)

#define tl_treap_smallest_where_g(r, t, type,sub, pred) do { \
	tl_treap_node *p_ = &(t)->header, *leftmost_ = NULL; \
	int dir_ = 0; \
	while(p_->ch[dir_] != &tl_treap_null) { \
		type* other = tl_treap_getnode(type,sub, (p_ = p_->ch[dir_])); \
		dir_ = !(pred); \
		if(dir_ == 0) leftmost_ = p_; \
	} \
	r = tl_treap_getnode(type,sub, leftmost_); \
} while(0)

#define tl_treap_find_g(r, t, type,sub, gt,eq) do { \
	tl_treap_node *p_ = &(t)->header; \
	int dir_ = 0; \
	while(p_->ch[dir_] != &tl_treap_null) { \
		type* other = tl_treap_getnode(type,sub, (p_ = p_->ch[dir_])); \
		dir_ = (gt); \
		if(!dir_ && (eq)) { (r) = other; break; } \
	} \
} while(0)

#define tl_treap_next_node_g(t, type,sub, el) do { \
	treap* t_ = (t); \
	tl_treap_node *el_ = &(el)->sub; \
	if(el_->ch[1] != &tl_treap_null) { \
		(el) = tl_treap_getnode(type,sub, tl_treap_leftmost(el_->ch[1])); \
	} else while(1) { \
		tl_treap_node* n_ = el_->parent; \
		if(n_ == &t_->header) { (el) = NULL; break; } \
		if(n_->ch[0] == el_) { (el) = tl_treap_getnode(type,sub, n_); break; } \
		el_ = n_; \
	} \
} while(0)

#define tl_treap_clear_g(t, type,sub) tl_treap_clear((t)->header.ch[0], offsetof(type,sub))

#define tl_treap_is_empty(t) ((t)->header.ch[0] == &tl_treap_null)

#define tl_def_treap(name, type,sub, gt,eq, ktype,kgt,keq) \
typedef struct name { \
	treap t; \
} name; \
TL_INLINE void name##_init(name* T) { tl_treap_init(&T->t); } \
TL_INLINE int name##_insert(name* T, type* el) { \
	tl_treap_insert_g(&T->t, type,sub, el, gt,eq); \
	return el != NULL; \
} \
TL_INLINE type* name##_find(name* T, ktype el) { \
	type* r = NULL; \
	tl_treap_find_g(r, &T->t, type,sub, kgt,keq); \
	return r; \
} \
TL_INLINE void name##_remove(name* T, type* el) { tl_treap_remove_g(&T->t, sub, el); } \
TL_INLINE void name##_clear(name* T) { tl_treap_clear_g(&T->t, type,sub); } \
TL_INLINE treap* name##_super(name* T) { return &T->t; }


#endif // UUID_A150C7AB6941491F46D3788208FD1F48
