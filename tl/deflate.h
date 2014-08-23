#ifndef UUID_28D1AF81252F4CF0217271A3585223F2
#define UUID_28D1AF81252F4CF0217271A3585223F2

#include "config.h"
#include "stream.h"
#include "inflate.h"

typedef struct tl_deflate {
	tl_byte_source in;
	tl_byte_sink out;
	int flush;
} tl_deflate;

TL_DEF_API tl_deflate* tl_def_create();
TL_DEF_API int tl_def_run(tl_deflate*);
TL_DEF_API void tl_def_destroy(tl_deflate*);

#endif // UUID_28D1AF81252F4CF0217271A3585223F2
