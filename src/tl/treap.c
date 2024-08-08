#include "treap.h"
#include <stdlib.h>

tl_treap_node tl_treap_null = {{&tl_treap_null, &tl_treap_null}, NULL, 0xffffffff};

/* TODO: Separate xorshift generator */
uint32_t tl_treap_genrand_int32(treap* self) {
  uint32_t t = self->x ^ (self->x << 11);
  self->x = self->y; self->y = self->z; self->z = self->w;
  return (self->w = self->w ^ (self->w >> 19) ^ (t ^ (t >> 8)));
}

void tl_treap_init_node(treap* t, tl_treap_node* el) {
	uint32_t p = tl_treap_genrand_int32(t);
	el->prio = p;
	el->ch[0] = el->ch[1] = &tl_treap_null;
}

void tl_treap_init(treap* t) {
	t->header.ch[0] = &tl_treap_null;
	t->header.prio = 0;
	t->x = 123456789;
	t->y = 362436069;
	t->z = 521288629;
	t->w = 88675123;
}

void tl_treap_restore_heap(tl_treap_node* p, tl_treap_node* el, int dir) {
	while(p->prio > el->prio) {
		tl_treap_node *oldparent = p->parent, *cc = el->ch[1-dir];

		(cc->parent = p)->ch[  dir] = cc;
		(p->parent = el)->ch[1-dir] = p;

		dir = (oldparent->ch[0] != p);
		p = oldparent;
	}

	p->ch[dir] = el;
	el->parent = p;
}

void tl_treap_remove(treap* t, tl_treap_node* el) {
	tl_treap_node *p = el->parent, *bubble;
	int dir = (p->ch[0] != el);
	(void)t; /* Unreferenced */

	while(1) {
		int dir2 = (el->ch[0]->prio <= el->ch[1]->prio);

		p->ch[dir] = (bubble = el->ch[1-dir2]);
		if(bubble == &tl_treap_null) break;

		el->ch[1-dir2] = bubble->ch[dir2];

		bubble->parent = p;
		p = bubble;
		dir = dir2;
	}
}

tl_treap_node* tl_treap_leftmost(tl_treap_node* p) {
	while(p->ch[0] != &tl_treap_null)
		p = p->ch[0];
	return p;
}

void tl_treap_clear(tl_treap_node* p, int offset) {
	if(p == &tl_treap_null) return;
	tl_treap_clear(p->ch[0], offset);
	tl_treap_clear(p->ch[1], offset);
	free((char*)p - offset);
}
