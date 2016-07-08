#ifndef TL_CHAR_H
#define TL_CHAR_H

#include "cstdint.h"
#include "config.h"

#define TL_CHAR_CNTRL	0x01
#define TL_CHAR_SPACE	0x02
#define TL_CHAR_PUNCT	0x04
#define TL_CHAR_DIGIT	0x08
#define TL_CHAR_XDIGIT	0x10
#define TL_CHAR_UPPER	0x20
#define TL_CHAR_LOWER	0x40
#define TL_CHAR_IDENT	0x80
#define TL_CHAR_ALPHA	(TL_CHAR_LOWER|TL_CHAR_UPPER)
#define TL_CHAR_ALNUM	(TL_CHAR_ALPHA|TL_CHAR_DIGIT)
#define TL_CHAR_GRAPH	(TL_CHAR_ALNUM|TL_CHAR_PUNCT)

/* Only pass -1 or 0..255 to these macros. Never pass a signed char! */
#define tl_char_isa(c, t)	((tl_char_bits+1)[(c)] & t)
#define tl_char_iscntrl(c)	tl_char_isa((c), TL_CHAR_CNTRL)
#define tl_char_isspace(c)	tl_char_isa((c), TL_CHAR_SPACE)
#define tl_char_ispunct(c)	tl_char_isa((c), TL_CHAR_PUNCT)
#define tl_char_isdigit(c)	tl_char_isa((c), TL_CHAR_DIGIT)
#define tl_char_isxdigit(c)	tl_char_isa((c), TL_CHAR_XDIGIT)
#define tl_char_isupper(c)	tl_char_isa((c), TL_CHAR_UPPER)
#define tl_char_islower(c)	tl_char_isa((c), TL_CHAR_LOWER)
#define tl_char_isident(c)	tl_char_isa((c), TL_CHAR_IDENT)
#define tl_char_isalpha(c)	tl_char_isa((c), TL_CHAR_ALPHA)
#define tl_char_isalnum(c)	tl_char_isa((c), TL_CHAR_ALNUM)
#define tl_char_isgraph(c)	tl_char_isa((c), TL_CHAR_GRAPH)

#define tl_char_toupper(c)	((c) - (tl_char_islower(c) >> 1))
#define tl_char_tolower(c)	((c) + tl_char_isupper(c))

TL_API const uint8_t tl_char_bits[257];

#endif // TL_CHAR_H
