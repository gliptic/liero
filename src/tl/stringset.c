#include "stringset.h"
#include "bits.h"

#include <string.h>

/* Adapted from LuaJIT */

#define tl_getu32(p) *((uint32_t const*)(p))

#define TL_MAX_STR_LEN (((size_t)-1) - sizeof(tl_string))

void tl_stringset_init(tl_stringset* S, size_t size) {
	S->table = calloc(size, sizeof(tl_string*));
	S->mask = size - 1;
	S->count = 0;
}

void tl_stringset_destroy(tl_stringset* S) {
	size_t i = 0;
	for (; i < S->mask + 1; ++i) {
		tl_string *s = S->table[i];
		while (s) {
			tl_string *n = s->next;
			free(s);
			s = n;
		}
	}
	free(S->table);
}

tl_string* tl_stringset_intern(tl_stringset* S, tl_string* str) {
	tl_string* o;
	uint32_t len = str->len, h = str->hash;

	o = S->table[h & S->mask];
	while(o != NULL) {
		if(o->len == len && memcmp(str, o->data, len) == 0) {
			if(o != str)
				tl_string_free(str);
			return o; /* Return existing string. */
		}
		o = o->next;
	}

	h &= S->mask;
	str->next = S->table[h];
	S->table[h] = str;
	return str;
}

tl_string* tl_stringset_add(tl_stringset* S, char const* str, size_t len) {
	uint32_t a, b, h = len;
	tl_string* o;

	// TODO: Check max length
	if(len > TL_MAX_STR_LEN)
		return NULL;

	if(len >= 4) {  /* Caveat: unaligned access! */
		a = tl_getu32(str);
		h ^= tl_getu32(str + len - 4);
		b = tl_getu32(str + (len >> 1) - 2);
		h ^= b; h -= tl_rol32(b, 14);
		b += tl_getu32(str + (len >> 2) - 1);
	} else if(len > 0) {
		a = *(uint8_t const*)str;
		h ^= *(uint8_t const*)(str + len - 1);
		b = *(uint8_t const*)(str + (len >> 1));
		h ^= b; h -= tl_rol32(b, 14);
	} else {
		a = b = 0;
	}

	a ^= h; a -= tl_rol32(h, 11);
	b ^= a; b -= tl_rol32(a, 25);
	h ^= b; h -= tl_rol32(b, 16);

	o = S->table[h & S->mask];
	while(o != NULL) {
		if(o->len == len && memcmp(str, o->data, len) == 0) {
			return o; /* Return existing string. */
		}
		o = o->next;
	}

	o = malloc(sizeof(tl_string) + len); // Null-terminator included in data size
	o->reserved0 = 0;
	o->len = len;
	o->hash = h;
	memcpy(o->data, str, len);
	o->data[len] = '\0';
	h &= S->mask;
	o->next = S->table[h];
	S->table[h] = o;
	return o;
}
