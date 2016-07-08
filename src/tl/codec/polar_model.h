#ifndef UUID_C1292269CF8E4B5FBEAFBD99543A1397
#define UUID_C1292269CF8E4B5FBEAFBD99543A1397

#include "prefix_code.h"
#include "model.h"

/*
typedef struct tl_polar_model
{
	tl_model base;
} tl_polar_model;
*/

#define TL_ALIGNED_BUFFER(name, type, size) type name[((size) + sizeof(type) - 1) / sizeof(type)]

#define tl_def_polar_model(maxsym, name) \
	TL_ALIGNED_BUFFER(name, void*, TL_MODEL_SIZE((maxsym)+1))

void tl_polar_model_rebuild(tl_model* self);

#define tl_polar_model_update(self) tl_model_update(self, tl_polar_model_rebuild)


#endif // UUID_C1292269CF8E4B5FBEAFBD99543A1397
