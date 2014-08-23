#include "test.h"

tl_decl_test(treap_insert)
tl_decl_test(treap_remove)
tl_decl_test(heap_insert)

#include <stdio.h>

void tl_test_begin(tl_test_context* ctx, char const* name) {
	ctx->result = 1;
	printf("Running %s... ", name);
}

void tl_test_end(tl_test_context* ctx, char const* name) {
	if (ctx->result) {
		printf("OK\n");
	} else {
		printf("ERROR\n");
	}
}

void tl_test_failed_assert(tl_test_context* ctx, char const* cond) {
	ctx->result = 0;
}

void tl_tests() {
	tl_test_context ctx;
	tl_test_run(treap_insert);
	tl_test_run(treap_remove);

	tl_test_run(heap_insert);
}
