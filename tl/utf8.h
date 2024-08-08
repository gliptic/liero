#ifndef TL_UTF8_H
#define TL_UTF8_H

#include "config.h"
#include "platform.h"
#include "cstdint.h"

#define TL_UTF8_ACCEPT 0
#define TL_UTF8_REJECT 12

TL_API const uint8_t utf8d[364];

TL_INLINE uint32_t
tl_utf8_decode(uint32_t* state, uint32_t* codep, uint32_t byte) {
  uint32_t type = utf8d[byte];

  *codep = (*state != TL_UTF8_ACCEPT) ?
    (byte & 0x3fu) | (*codep << 6) :
    (0xff >> type) & (byte);

  *state = utf8d[256 + *state + type];
  return *state;
}

#endif // TL_UTF8_H
