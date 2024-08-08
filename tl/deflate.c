#include "deflate.h"

typedef struct tl_deflate_source {
	tl_deflate base;

} tl_deflate_source;

static void tl_def_init_(tl_deflate_source* self)
{

}

tl_deflate* tl_def_create()
{
	return NULL;
}

int tl_def_run(tl_deflate*);
void tl_def_destroy(tl_deflate*);