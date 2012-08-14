#include "mixer.h"
#include "tl/vector.h"
#include "tl/memory.h"
#include <string.h>
//#include "tl/riff.h"
#include <malloc.h>

typedef struct channel
{
	// Mutator read/write
	uint32_t id;

	// Mutator write once, mixer read
	uint32_t flags;

	// Mutator write once, mixer read
	sfx_sound* sound;

	// Mutator write once, mixer read/write
	uint32_t pos;

	uint64_t soundpos, stride;
	uint32_t volumes;
} channel;

typedef struct sfx_mixer
{
	uint32_t next_id;
	channel channel_states[CHANNEL_COUNT];

	uint32_t write_pos, tail_pos;
	uint32_t channels[CHANNEL_COUNT];

	int32_t base_frame;
	int initialized;
	double output_latency;
	double sample_rate;
} sfx_mixer;

typedef struct sfx_sound 
{
	tl_vector samples;
} sfx_sound;

/*
static uint32_t read_chunk_header(tl_byte_source_pullable* src, uint32_t* size)
{
	uint32_t id = tl_bs_pull32_def0_le(src);
	*size = tl_bs_pull32_def0_le(src);
	return id;
}

sfx_sound* sfx_load_wave(tl_byte_source_pullable* src)
{
	sfx_sound* snd = calloc(1, sizeof(sfx_sound));
	uint32_t total_size;
	uint32_t formatTag, channels, samplesPerSec, avgBytesPerSec, blockAlign, bitsPerSample;
	if(read_chunk_header(src, &total_size) != RIFF_SIGN('R', 'I', 'F', 'F'))
		goto fail;

	if(tl_bs_pull32_def0_le(src) != RIFF_SIGN('W', 'A', 'V', 'E'))
		goto fail;

	while(!tl_bs_check_pull(src))
	{
		uint32_t size;
		uint32_t id = read_chunk_header(src, &size);

		switch(id)
		{
			case RIFF_SIGN('f', 'm', 't', ' '):
			{
				formatTag = tl_bs_pull16_def0_le(src);
				channels = tl_bs_pull16_def0_le(src);
				samplesPerSec = tl_bs_pull32_def0_le(src);
				avgBytesPerSec = tl_bs_pull32_def0_le(src);
				blockAlign = tl_bs_pull16_def0_le(src);
				bitsPerSample = tl_bs_pull16_def0_le(src);

				if(formatTag != 1)
					goto fail; // Not PCM
				if(channels != 1)
					goto fail; // Not mono
				if(samplesPerSec != 44100)
					goto fail; // Not 44100 Hz
				if(bitsPerSample != 16)
					goto fail; // Not 16-bit

				tl_bs_pull_skip(src, size - (2+2+4+4+2+2));
				break;
			}

			case RIFF_SIGN('d', 'a', 't', 'a'):
			{
				uint32_t i;
				for(i = 0; i < size / 2; ++i)
					tl_pushback(snd->samples, uint16_t, tl_bs_pull16_def0_le(src));
				return snd; // Got data
			}

			default:
			{
				tl_bs_pull_skip(src, size);
				break;
			}
		}
	}

fail:
	free(snd);
	return NULL;
}

*/

#define CHECK_HANDLE(h) (((self)->channel_states[(h)&0xffff].id == (h)) ? ((h)&0xffff) : -1)

sfx_mixer* sfx_mixer_create(void)
{
	sfx_mixer* self = calloc(1, sizeof(sfx_mixer));
	uint32_t i;
	for(i = 0; i < CHANNEL_COUNT; ++i)
		self->channels[i] = i;

	return self;
}

static void mixer_swap(sfx_mixer* self, uint32_t idx1, uint32_t idx2)
{
	uint32_t temp = self->channels[idx1];
	self->channels[idx1] = self->channels[idx2];
	self->channels[idx2] = temp;
}

static int add_channel(int16_t* out, unsigned long frame_count, uint32_t now, channel* ch)
{
	sfx_sound* sound = ch->sound;
	uint32_t pos = ch->pos;
	int32_t scaler;
	uint32_t cur;

	// TODO: Get rid of undefined cast
	int32_t diff = (int32_t)(pos - now);
	int32_t relbegin = diff;
	uint32_t soundlen = tl_vector_size(sound->samples);

	if(relbegin < 0)
		relbegin = 0;

	scaler = (ch->volumes & 0x1fff);

	for(cur = (uint32_t)relbegin; cur < frame_count; ++cur)
	{
		int32_t samp = out[cur*2];
		int32_t soundsamp = tl_vector_el(sound->samples, int16_t, (uint32_t)(ch->soundpos >> 32));
		samp += (soundsamp * scaler) >> 12;
		if(samp < -32768)
			samp = -32768;
		else if(samp > 32767)
			samp = 32767;
		out[cur*2] = (int32_t)(samp);
		out[cur*2+1] = (int32_t)(samp);
		ch->soundpos += ch->stride;
		if((uint32_t)(ch->soundpos >> 32) >= soundlen)
		{
		/*
			if(ch.flags & Channel::Looping)
				ch.soundpos = 0.0;
			else*/
				return 0;
		}
	}

	return 1;
}

void sfx_mixer_mix(sfx_mixer* self, void* output, unsigned long frame_count, uint32_t now)
{
	int16_t* out = (int16_t*)(output);
	uint32_t write_pos = self->write_pos;
	uint32_t c;
	memset(out, 0, frame_count * 2 * sizeof(int16_t));

	// TODO: Handle discontinuous mixing.
	// i.e. if (now + frame_count) in one call differs from (now) in the next,
	// we need to advance soundpos for channels to be accurate.

	for(c = self->tail_pos; c != write_pos; )
	{
		uint32_t ch_idx = self->channels[c];
		channel* ch = self->channel_states + ch_idx;

		if(!add_channel(out, frame_count, now, ch))
		{
			// Remove
			mixer_swap(self, c, self->tail_pos);
			self->tail_pos = (self->tail_pos + 1) & (CHANNEL_COUNT-1);
		}

		c = (c + 1) & (CHANNEL_COUNT-1);
	}

	self->base_frame += frame_count;
}

/*
void sfx_mixer_fill(sfx_stream* str, uint32_t start, uint32_t frames)
{
	sfx_mixer* self = (sfx_mixer*)str->ud;
	sfx_mixer_mix(self, str->buffer, frames, start);
}*/

void sfx_set_volume(sfx_mixer* self, uint32_t h, double speed)
{
	int ch = CHECK_HANDLE(h);
	if(ch < 0) return;

	self->channel_states[ch].stride = (speed * 0x100000000);
}

static uint32_t active_channels(sfx_mixer* self)
{
	TL_READ_SYNC();
	return (self->write_pos - self->tail_pos) & (CHANNEL_COUNT-1);
}

uint32_t free_channels(sfx_mixer* self)
{
	return CHANNEL_COUNT - active_channels(self);
}

uint32_t sfx_mixer_add(sfx_mixer* self, sfx_sound* snd, uint32_t time)
{
	if(free_channels(self) > 1)
	{
		uint32_t id;
		uint32_t ch_idx = self->channels[self->write_pos];
		channel* ch = self->channel_states + ch_idx;
		ch->sound = snd;
		ch->flags = 0;
		ch->pos = time;
		ch->soundpos = 0;
		ch->stride = 0x100000000ull;
		ch->volumes = 0x10001000;

		id = ((self->next_id++) << 16) | ch_idx;
		ch->id = id;
		TL_WRITE_SYNC(); // Make sure ch->id is written before self->write_pos

		self->write_pos = (self->write_pos + 1) & (CHANNEL_COUNT-1);
		return id;
	}

	return 0;
}

#undef CHECK_HANDLE
