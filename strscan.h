/*
** String scanning.
** Copyright (C) 2005-2012 Mike Pall. See Copyright Notice in luajit.h
*/

#ifndef _LJ_STRSCAN_H
#define _LJ_STRSCAN_H

//#include "tl_obj.h"
#include "config.h"
#include "cstdint.h"

/* Options for accepted/returned formats. */
#define STRSCAN_OPT_TOINT	0x01  /* Convert to int32_t, if possible. */
#define STRSCAN_OPT_TONUM	0x02  /* Always convert to double. */
#define STRSCAN_OPT_IMAG	0x04
#define STRSCAN_OPT_LL		0x08
#define STRSCAN_OPT_C		0x10

/* Returned format. */
typedef enum {
  STRSCAN_ERROR,
  STRSCAN_NUM, STRSCAN_IMAG,
  STRSCAN_INT, STRSCAN_U32, STRSCAN_I64, STRSCAN_U64,
} StrScanFmt;

typedef union tl_value {
	uint64_t u64;
	double n;
	int32_t i;
} tl_value;

#define setnumV(o, x)	((o)->n = (x))
#define setnanV(o)		((o)->u64 = TL_U64x(fff80000,00000000))
#define setpinfV(o)		((o)->u64 = TL_U64x(7ff00000,00000000))
#define setminfV(o)		((o)->u64 = TL_U64x(fff00000,00000000))

TL_API StrScanFmt tl_strscan_scan(const uint8_t *p, tl_value *o, uint32_t opt);
TL_API int tl_strscan_num(char const *str, size_t len, tl_value *o);
TL_API int tl_strscan_number(char const *str, size_t len, tl_value *o);

#endif
