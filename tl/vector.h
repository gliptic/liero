#ifndef UUID_C8B889F6B0254CE3F7AE58B920794A20
#define UUID_C8B889F6B0254CE3F7AE58B920794A20

#include <stddef.h>
#include <malloc.h>
#include <assert.h>
#include "cstdint.h"
#include "platform.h"

#define tl_vector_new(v, t, n) do { \
	size_t n_ = (n); \
	tl_vector* v_ = &(v); \
	v_->cap = n_; \
	v_->size = 0; \
	if(n_ > SIZE_MAX/sizeof(t)) \
		v_->impl = NULL; \
	else \
		v_->impl = malloc(sizeof(t)*(n_)); \
} while(0)

#define tl_vector_new_empty(v) do { \
	tl_vector* v_ = &(v); \
	v_->cap = 0; \
	v_->size = 0; \
	v_->impl = NULL; \
} while(0)

#define tl_vector_pushback(v, t, e) do { \
	tl_vector* p_v_ = &(v); \
	size_t s_ = tl_vector_size(*p_v_); \
	if(s_ >= p_v_->cap) tl_vector_enlarge(*p_v_, t, 1); \
	((t*)p_v_->impl)[s_++] = (e); \
	p_v_->size = s_; \
} while(0)

#define tl_vector_foreach(v, t, var, body) do { \
	tl_vector* fv_ = &(v); \
	t* var = (t*)fv_->impl; \
	t* e_ = ((t*)var + fv_->size); \
	for(; var != e_; ++var) \
		body \
} while(0)x

#define tl_vector_filtereach(v, t, var, body) do { \
	tl_vector* fi_v_##var = &(v); \
	size_t idx_ = 0; \
	for(; idx_ < fi_v_##var->size; ++idx_) { \
		int remove = 0; \
		t* var = (t*)fi_v_##var->impl + idx_; \
		body \
		if(remove) tl_vector_remove_reorder(*fi_v_##var, t, idx_);  \
		else ++idx_; \
	} \
} while(0)

#define tl_vector_remove_reorder(v, t, idx) do { \
	tl_vector* rr_v_ = &(v); \
	t* base_ = (t*)rr_v_->impl; \
	size_t s_ = tl_vector_size(*rr_v_); \
	size_t rr_idx_ = (idx); \
	assert(rr_idx_ < s_); \
	base_[rr_idx_] = base_[s_ - 1]; \
	--rr_v_->size; \
} while(0)

#define tl_vector_reserve(v, t, newcap) do { \
	tl_vector* e_v_ = &(v); \
	void* new_impl_; \
	size_t newcap_ = (newcap); \
	if(newcap_ > e_v_->cap) { \
		if(newcap_ > SIZE_MAX/sizeof(t) \
		|| !(new_impl_ = realloc(e_v_->impl, sizeof(t)*newcap_))) { \
			free(e_v_->impl); new_impl_ = NULL; \
		} \
		e_v_->impl = new_impl_; \
		e_v_->cap = newcap_; \
	} \
} while(0)

#define tl_vector_enlarge(v, t, extra) do { \
	tl_vector* e_v_ = &(v); \
	void* new_impl_; \
	size_t newcap_ = e_v_->size * 2 + (extra); \
	if(newcap_ > SIZE_MAX/sizeof(t) \
	|| !(new_impl_ = realloc(e_v_->impl, sizeof(t)*newcap_))) { \
		free(e_v_->impl); new_impl_ = NULL; \
	} \
	e_v_->impl = new_impl_; \
	e_v_->cap = newcap_; \
} while(0)

#define tl_vector_idx(v, t, i) ((t*)(v).impl + (i))
#define tl_vector_el(v, t, i) (*tl_vector_idx(v,t,i))
#define tl_vector_size(v) ((v).size)
#define tl_vector_post_enlarge(v, t, n) ((v).size += (n))
#define tl_vector_free(v) free((v).impl)

typedef struct tl_vector {
	size_t cap;
	size_t size;
	void* impl;
} tl_vector;

#define tl_def_vector(name, t) \
typedef struct name { \
	tl_vector v; \
} name;  \
TL_INLINE void name##_init_empty(name* V) { tl_vector_new_empty(V->v); } \
TL_INLINE void name##_init(name* V, size_t n) { tl_vector_new(V->v, t, n); } \
TL_INLINE t name##_el(name* V, size_t i) { return tl_vector_el(V->v, t, i); } \
TL_INLINE t* name##_idx(name* V, size_t i) { return tl_vector_idx(V->v, t, i); } \
TL_INLINE size_t name##_size(name* V) { return tl_vector_size(V->v); } \
TL_INLINE void name##_destroy(name* V) { tl_vector_free(V->v); } \
TL_INLINE void name##_pushback(name* V, t e) { tl_vector_pushback(V->v, t, e); } \
TL_INLINE tl_vector* name##_super(name* V, t e) { return &V->v; }

#endif // UUID_C8B889F6B0254CE3F7AE58B920794A20
