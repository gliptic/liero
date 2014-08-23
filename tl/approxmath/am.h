#ifndef UUID_7218AF0FB69C484F46F1C185F0A5EF73
#define UUID_7218AF0FB69C484F46F1C185F0A5EF73

#include "../config.h"

typedef struct am_pair { float first, second; } am_pair;

TL_AM_API am_pair am_sincosf(float x);
TL_AM_API float   am_sinf(float x);
TL_AM_API float   am_cosf(float x);
/*TL_AM_API*/ float   am_asinf(float x);
/*TL_AM_API*/ float   am_acosf(float x);

/*TL_AM_API*/ float am_sinf_2(float x);

TL_AM_API float am_tanf(float x);
/*TL_AM_API*/ float am_tanf_2(float x);

TL_AM_API float am_expf(float x);
TL_AM_API float am_exp2f(float x);
/*TL_AM_API*/ float am_expf_2(float x);
/*TL_AM_API*/ float cephes_expf(float x);

TL_AM_API float am_powf(float x, float y);

float am_sinf_inline(float x);
float am_sinf_intrin(float x);

#endif // UUID_7218AF0FB69C484F46F1C185F0A5EF73
