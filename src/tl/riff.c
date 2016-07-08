#include "riff.h"

uint64 riff_push_hdr(tl_byte_sink_pushable* self, uint32 sign, uint32 len)
{
	tl_bs_push32_le(self, sign);
	tl_bs_push32_le(self, len);
	return tl_bs_tell_sink(self);
}

uint64 riff_push_riff_hdr(tl_byte_sink_pushable* self, uint32 sign, uint32 len)
{
	uint64 p = riff_push_hdr(self, RIFF_SIGN('R','I','F','F'), len + 4);
	tl_bs_push32_le(self, sign);
	return p;
}

uint64 riff_push_list_hdr(tl_byte_sink_pushable* self, uint32 sign, uint32 len)
{
	uint64 p = riff_push_hdr(self, RIFF_SIGN('L','I','S','T'), len + 4);
	tl_bs_push32_le(self, sign);
	return p;
}

void riff_patch_hdr_len(tl_byte_sink_pushable* self, uint64 org)
{
	uint64 cur = tl_bs_tell_sink(self);
	uint64 diff = cur - org;
	tl_bs_seek_sink(self, org - 4);
	tl_bs_push32_le(self, (uint32)diff);
	tl_bs_seek_sink(self, cur);
}
