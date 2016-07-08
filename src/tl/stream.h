#ifndef UUID_F3740CE70E6B45893FAD7C9B2BC6A2C0
#define UUID_F3740CE70E6B45893FAD7C9B2BC6A2C0

#include "config.h"
#include "platform.h"
#include "cstdint.h"
#include "vector.h"
#include <stddef.h>

typedef struct tl_byte_source {
	uint8* buf;
	uint8* buf_end;
} tl_byte_source;

typedef struct tl_byte_source_pullable {
	uint8* buf;
	uint8* buf_end;
	void* ud;
	int  (*pull)(struct tl_byte_source_pullable*);
	void (*free)(struct tl_byte_source_pullable*);
} tl_byte_source_pullable;

typedef struct tl_byte_sink {
	uint8* out_start;
	uint8* out;
	uint8* out_end;
} tl_byte_sink;

typedef struct tl_byte_sink_pushable {
	uint8* out_start;
	uint8* out;
	uint8* out_end;
	void* ud;
	int      (*push)(struct tl_byte_sink_pushable*);
	int      (*seek)(struct tl_byte_sink_pushable*, uint64_t);
	uint64_t (*tell)(struct tl_byte_sink_pushable*);
	void     (*free)(struct tl_byte_sink_pushable*);
} tl_byte_sink_pushable;

#define tl_bs_check(source) ((source)->buf != (source)->buf_end)
#define tl_bs_left(source) ((size_t)((source)->buf_end - (source)->buf))
#define tl_bs_unsafe_get(source) (*(source)->buf++)

// NOTE! These return an error code, not a boolean.
#define tl_bs_check_pull(source) ((source)->buf != (source)->buf_end ? 0 : (source)->pull(source))
#define tl_bs_pull_def0(source) (tl_bs_check_pull(source) ? 0 : tl_bs_unsafe_get(source))

TL_INLINE uint16 tl_bs_pull16_def0(tl_byte_source_pullable* self)
{
	uint8 x = tl_bs_pull_def0(self);
	return (x << 8) + tl_bs_pull_def0(self);
}

TL_INLINE uint32 tl_bs_pull32_def0(tl_byte_source_pullable* self)
{
	uint16 x = tl_bs_pull16_def0(self);
	return (x << 16) + tl_bs_pull16_def0(self);
}

TL_INLINE uint16 tl_bs_pull16_def0_le(tl_byte_source_pullable* self)
{
	uint8 x = tl_bs_pull_def0(self);
	return (tl_bs_pull_def0(self) << 8) + x;
}

TL_INLINE uint32 tl_bs_pull32_def0_le(tl_byte_source_pullable* self)
{
	uint16 x = tl_bs_pull16_def0_le(self);
	return (tl_bs_pull16_def0_le(self) << 16) + x;
}

TL_INLINE void tl_bs_pull_skip(tl_byte_source_pullable* self, int n)
{
	// TODO: Optimize
	int i;
	for(i = 0; i < n; ++i)
		(void)tl_bs_pull_def0(self);
}

TL_STREAM_API void tl_bs_init_source(tl_byte_source_pullable* self);
TL_STREAM_API int  tl_bs_file_source(tl_byte_source_pullable* src, char const* path);
TL_STREAM_API int  tl_bs_pulln(tl_byte_source_pullable* src, uint8* data, int n);
TL_STREAM_API void tl_bs_free(tl_byte_source_pullable* src);

// Sinks

#define tl_bs_check_push(sink)    ((sink)->out != (sink)->out_end ? 0 : (sink)->push(sink))
#define tl_bs_check_sink(sink)    ((sink)->out != (sink)->out_end)
#define tl_bs_push(sink, b)       (tl_bs_check_push(sink) ? 0 : tl_bs_unsafe_put(sink,b))
#define tl_bs_flush(sink)         if((sink)->out != (sink)->out_start) (sink)->push(sink); else (void)0
#define tl_bs_unsafe_put(sink, b) (*(sink)->out++ = (b))

TL_INLINE void tl_bs_push16(tl_byte_sink_pushable* self, uint16 x)
{
	tl_bs_push(self, (x>>8));
	tl_bs_push(self, x & 0xff);
}

TL_INLINE void tl_bs_push32(tl_byte_sink_pushable* self, uint32 x)
{
	tl_bs_push16(self, (x>>16) & 0xffff);
	tl_bs_push16(self, x & 0xffff);
}

TL_INLINE void tl_bs_push16_le(tl_byte_sink_pushable* self, uint16 x)
{
	tl_bs_push(self, x & 0xff);
	tl_bs_push(self, (x>>8));
}

TL_INLINE void tl_bs_push32_le(tl_byte_sink_pushable* self, uint32 x)
{
	tl_bs_push16_le(self, x & 0xffff);
	tl_bs_push16_le(self, (x>>16) & 0xffff);
}

TL_STREAM_API int    tl_bs_seek_sink(tl_byte_sink_pushable* self, uint64 p);
TL_STREAM_API int    tl_bs_pushn(tl_byte_sink_pushable* self, uint8 const* data, int n);
TL_STREAM_API uint64 tl_bs_tell_sink(tl_byte_sink_pushable* self);
TL_STREAM_API void   tl_bs_init_sink(tl_byte_sink_pushable* self);
TL_STREAM_API int    tl_bs_file_sink(tl_byte_sink_pushable* sink, char const* path);
TL_STREAM_API void   tl_bs_free_sink(tl_byte_sink_pushable* self);

/* Memory stream */
TL_STREAM_API int       tl_bs_mem_sink(tl_byte_sink_pushable* sink);
TL_STREAM_API tl_vector tl_bs_mem_release_vector(tl_byte_sink_pushable* sink);

#endif // UUID_F3740CE70E6B45893FAD7C9B2BC6A2C0
