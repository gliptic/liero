#ifndef TL_TEST_H
#define TL_TEST_H

#include "../config.h"

typedef struct tl_test_context {
	int result;
} tl_test_context;

TL_API void tl_test_begin(tl_test_context* ctx, char const* name);
TL_API void tl_test_end(tl_test_context* ctx, char const* name);
TL_API void tl_test_failed_assert(tl_test_context* ctx, char const* cond);

#define tl_test_assert(cond) do { \
	if(!(cond)) tl_test_failed_assert(ctx, #cond); \
} while(0)

#define tl_decl_test(name) void tl_test_##name(tl_test_context* ctx);

#define tl_test(name) void tl_test_##name(tl_test_context* ctx) { \
	char const* _name = #name; \
	tl_test_begin(ctx, _name);

#define tl_end_test() \
	tl_test_end(ctx, _name); \
}

#define tl_test_run(name) tl_test_##name(&ctx)

TL_API void tl_tests();

#endif // TL_TEST_H