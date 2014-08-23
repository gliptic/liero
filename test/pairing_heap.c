#include "../pairing_heap.h"

#include "test.h"

typedef struct heap_testnode {
	tl_ph_node node;
	int v;
} heap_testnode;

tl_def_pairing_heap(testheap,
	heap_testnode,node, (a->v > b->v))

tl_test(heap_insert) {
	testheap tt;
	heap_testnode tn10, tn11, tn9;
	heap_testnode* r;
	tn10.v = 10;
	tn11.v = 11;
	tn9.v = 9;
	testheap_init(&tt);
	testheap_insert(&tt, &tn10);
	testheap_insert(&tt, &tn11);
	testheap_insert(&tt, &tn9);

	tl_test_assert(!testheap_is_empty(&tt));

	r = testheap_min(&tt);
	tl_test_assert(r == &tn9);

	r = testheap_unlink_min(&tt);
	tl_test_assert(r == &tn9);

	r = testheap_unlink_min(&tt);
	tl_test_assert(r == &tn10);

	r = testheap_unlink_min(&tt);
	tl_test_assert(r == &tn11);

	tl_test_assert(testheap_is_empty(&tt));
} tl_end_test()