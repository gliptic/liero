#ifndef TL_PAIRING_HEAP
#define TL_PAIRING_HEAP

#include "config.h"
#include "cstdint.h"
#include <stddef.h>

typedef struct tl_ph_node {
	struct tl_ph_node *left_child, **prev_next, *right_sibling;
} tl_ph_node;

typedef struct tl_pairing_heap {
	tl_ph_node* root;
} tl_pairing_heap;

extern tl_ph_node tl_ph_null;

TL_API tl_ph_node* tl_ph_combine_siblings_(tl_ph_node* el, tl_ph_node* (*complink)(tl_ph_node* a, tl_ph_node* b));

#define tl_ph_downcast(type,sub, n) ((type*)((char*)(n) - offsetof(type,sub)))

#define tl_def_pairing_heap(name, type,sub, gt) \
typedef struct name { \
	tl_ph_node* root; \
} name; \
TL_INLINE tl_ph_node* name##_complink(tl_ph_node* a_, tl_ph_node* b_) { \
	type* a = tl_ph_downcast(type,sub, a_); \
	type* b = tl_ph_downcast(type,sub, b_); \
	if((gt)) { tl_ph_node* temp = b_; b_ = a_; a_ = temp; } \
	a_->left_child->prev_next = &b_->right_sibling; \
	b_->right_sibling = a_->left_child; \
	b_->prev_next = &a_->left_child; \
	a_->left_child = b_; \
	return a_; \
} \
TL_INLINE void name##_init(name* H) { \
	H->root = &tl_ph_null; \
} \
TL_INLINE void name##_insert(name* H, type* n) { \
	n->sub.left_child = &tl_ph_null; \
	H->root = (H->root != &tl_ph_null ? name##_complink(H->root, &n->sub) : &n->sub); \
} \
TL_INLINE type* name##_min(name* H) { \
	return tl_ph_downcast(type,sub, H->root); \
} \
TL_INLINE int name##_is_empty(name* H) { \
	return H->root == &tl_ph_null; \
} \
TL_INLINE type* name##_unlink_min(name* H) { \
	tl_ph_node *ret = H->root; \
	tl_ph_node *left_child = ret->left_child; \
	H->root = tl_ph_combine_siblings_(left_child, name##_complink); \
	return tl_ph_downcast(type,sub, ret); \
}

#endif // TL_PAIRING_HEAP
