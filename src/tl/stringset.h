#ifndef UUID_94DA9D3860F048018F51DE4679A9DE11
#define UUID_94DA9D3860F048018F51DE4679A9DE11

#include "string.h"

typedef struct tl_stringset {
	tl_string** table;
	size_t mask;
	size_t count;
} tl_stringset;

void tl_stringset_init(tl_stringset* S, size_t size);
void tl_stringset_destroy(tl_stringset* S);

tl_string* tl_stringset_add(tl_stringset* S, char const* str, size_t len);

/* Takes ownership of str */
tl_string* tl_stringset_intern(tl_stringset* S, tl_string* str);

#endif /* UUID_94DA9D3860F048018F51DE4679A9DE11 */

