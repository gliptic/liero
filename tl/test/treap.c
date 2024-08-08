#include "../treap.h"

#include "test.h"

typedef struct treap_testnode {
	tl_treap_node node;
	int v;
} treap_testnode;

tl_def_treap(testtreap,
	treap_testnode,node, (el->v > other->v), (el->v == other->v),
	int, (el > other->v), (el == other->v))

tl_test(treap_insert) {
	testtreap tt;
	treap_testnode tn;
	treap_testnode* r;
	tn.v = 10;
	testtreap_init(&tt);
	testtreap_insert(&tt, &tn);

	r = testtreap_find(&tt, 10);
	tl_test_assert(r != NULL);

	r = testtreap_find(&tt, 11);
	tl_test_assert(r == NULL);
} tl_end_test()

tl_test(treap_remove) {
	int i;
	testtreap tt;
	treap_testnode tn[64];
	treap_testnode* r;
	testtreap_init(&tt);

	for(i = 0; i < 64; ++i)
	{
		tn[i].v = i;
		testtreap_insert(&tt, &tn[i]);
	}

	testtreap_remove(&tt, &tn[7]);
	tl_test_assert(testtreap_find(&tt, 7) == NULL);
	tl_test_assert(testtreap_find(&tt, 8)->v == 8);
} tl_end_test()