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

#endif // TL_STRING_H
