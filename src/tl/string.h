#ifndef TL_STRING_H
#define TL_STRING_H

#include <stddef.h>
#include <stdlib.h>
#include "cstdint.h"

typedef struct tl_string {
	struct tl_string* next;
	size_t len;
	uint32_t hash;
	uint8_t reserved0, reserved1, reserved2, reserved3;
	char data[1];
} tl_string;

#define tl_string_free(str) free(str)

struct tl_strdata {
	uint8_t len;
	uint8_t d[16];
};

struct tl_strbuf {
	uint8_t const* p;
	uint32_t cap;
};

#define TL_STRREF_IMPL union { uint8_t d[16]; tl_strbuf b; } u; uint32_t len

typedef struct tl_strref {
	TL_STRREF_IMPL;
};

typedef struct tl_str {
	TL_STRREF_IMPL;
};

#define tl_strdata(s) ((s)->len <= )

#endif // TL_STRING_H
