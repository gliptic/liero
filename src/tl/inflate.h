#ifndef UUID_771F42532AD94623B1E473A76A1A9FB0
#define UUID_771F42532AD94623B1E473A76A1A9FB0

#include "config.h"
#include "stream.h"

typedef struct tl_inflate {
	tl_byte_source in;
	tl_byte_sink out;
	int flush;
} tl_inflate;

#define ZERR_OK (0)
#define ZERR_UNDERFLOW (1)
#define ZERR_OVERFLOW (2)
#define ZERR_BAD_CODELENGTHS (-1)
#define ZERR_BAD_HUFFMAN_CODE (-2)
#define ZERR_BAD_DISTANCE (-3)
#define ZERR_STREAM_CORRUPT (-4)
#define ZERR_BAD_HEADER (-5)
#define ZERR_PRESET_DICT_NOT_SUPPORTED (-6)
#define ZERR_UNSUPPORTED_ENCODING (-7)

TL_INF_API tl_inflate* tl_inf_create();
TL_INF_API int tl_inf_run(tl_inflate*);
TL_INF_API void tl_inf_destroy(tl_inflate*);

#endif // UUID_771F42532AD94623B1E473A76A1A9FB0
