#include "mixer.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>

using channel = struct Channel {
  // Mutator write once, mutator read, mixer write
  void* id;

  // Mutator write, mixer read
  uint32_t flags;

  // Mutator write once, mixer read
  sfx_sound* sound;

  // Mutator write once, mixer read/write
  uint32_t pos;

  uint64_t soundpos, stride;
  uint32_t volumes;

  /*
  Mutator can change flags from 0xFFFFFFFF, signaling a new active channel
  Mixer or mutator can change flags to 0xFFFFFFFF, signaling a new inactive channel
  */
};

struct SfxMixer {
  channel channel_states[CHANNEL_COUNT];

  int32_t base_frame;
  int initialized;
  double output_latency;
  double sample_rate;
};

struct SfxSound {
  std::vector<int16_t> samples;
};

int32_t SfxMixerNow(sfx_mixer* mixer) { return mixer->base_frame; }

sfx_sound* SfxNewSound(std::size_t samples) {
  auto* snd = new sfx_sound();

  snd->samples = std::vector<int16_t>(samples);
  return snd;
}

void SfxFreeSound(sfx_sound* snd) { delete snd; }

std::vector<int16_t>& SfxSoundData(sfx_sound* snd) { return snd->samples; }

// Unsigned-typed: the comparisons against `channel::flags` (a uint32_t) go
// through std::cmp_equal, which is signedness-aware — a signed -1 would
// compare unequal to the unsigned 0xFFFFFFFF stored in `flags` and silently
// leave every channel "active" with a null `sound`.
#define INACTIVE_FLAGS 0xFFFFFFFFu
#define MK_HANDLE(num, idx) ((uint32)(((num) << 8) + (idx)))
#define CHECK_HANDLE(h) (((self)->channel_states[(h) & 0xff].id == (h)) ? ((h) & 0xff) : -1)

sfx_mixer* SfxMixerCreate() {
  auto* self = new sfx_mixer();
  uint32_t i = 0;
  for (i = 0; i < CHANNEL_COUNT; ++i) {
    self->channel_states[i].flags = INACTIVE_FLAGS;
  }

  return self;
}

static int AddChannel(int16_t* out, unsigned long frame_count, uint32_t now, channel* ch) {
  sfx_sound* sound = ch->sound;
  uint32_t const kPos = ch->pos;
  int32_t scaler = 0;
  uint32_t cur = 0;

  // TODO: Get rid of undefined cast
  auto const kDiff = static_cast<int32_t>(kPos - now);
  int32_t relbegin = kDiff;
  uint32_t const kSoundlen = sound->samples.size();
  uint32_t const kFlags = ch->flags;

  relbegin = std::max(relbegin, 0);
  if (std::cmp_equal(kFlags, INACTIVE_FLAGS)) {
    return 0;  // Stop
  }

  scaler = (ch->volumes & 0x1fff);

  for (cur = static_cast<uint32_t>(relbegin); cur < frame_count; ++cur) {
    int32_t samp = out[cur];
    int32_t const kSoundsamp = sound->samples[static_cast<uint32_t>(ch->soundpos >> 32)];
    samp += (kSoundsamp * scaler) >> 12;
    if (samp < -32768) {
      samp = -32768;
    } else if (samp > 32767) {
      samp = 32767;
    }
    out[cur] = samp;
    ch->soundpos += ch->stride;
    if (static_cast<uint32_t>(ch->soundpos >> 32) >= kSoundlen) {
      if (kFlags & SFX_SOUND_LOOP) {
        ch->soundpos = 0;
      } else {
        return 0;
      }
    }
  }

  return 1;
}

void SfxMixerMix(sfx_mixer* self, void* output, unsigned long frame_count) {
  auto* out = static_cast<int16_t*>(output);
  uint32_t c = 0;
  memset(out, 0, frame_count * sizeof(int16_t));

  // TODO: Handle discontinuous mixing.
  // i.e. if (now + frame_count) in one call differs from (now) in the next,
  // we need to advance soundpos for channels to be accurate.

  for (c = 0; c < CHANNEL_COUNT; ++c) {
    channel* ch = self->channel_states + c;

    if (std::cmp_not_equal(ch->flags, INACTIVE_FLAGS)) {
      if (!AddChannel(out, frame_count, self->base_frame, ch)) {
        // Remove
        SDL_MemoryBarrierAcquire();
        ch->flags = INACTIVE_FLAGS;
        ch->id = nullptr;
        SDL_MemoryBarrierRelease();  // Make ch->flags and ch->id write visible
      }
    }
  }

  self->base_frame += frame_count;
}

/*
void sfx_mixer_fill(sfx_stream* str, uint32_t start, uint32_t frames)
{
        sfx_mixer* self = (sfx_mixer*)str->ud;
        sfx_mixer_mix(self, str->buffer, frames, start);
}*/

static int FindChannel(sfx_mixer* self, void* h) {
  uint32_t c = 0;

  for (c = 0; c < CHANNEL_COUNT;) {
    if (self->channel_states[c].id == h &&
        std::cmp_not_equal(self->channel_states[c].flags, INACTIVE_FLAGS)) {
      return static_cast<int>(c);
    }
    ++c;
  }

  return -1;
}

static int FindFreeChannel(sfx_mixer* self) {
  uint32_t c = 0;

  for (c = 0; c < CHANNEL_COUNT;) {
    if (std::cmp_equal(self->channel_states[c].flags, INACTIVE_FLAGS)) {
      return static_cast<int>(c);
    }
    ++c;
  }

  return -1;
}

void SfxSetVolume(sfx_mixer* self, void* h, double volume) {
  uint32_t v = 0;
  int const kCh = FindChannel(self, h);
  if (kCh < 0) {
    return;
  }

  v = static_cast<uint32_t>(volume * 0x1000);

  self->channel_states[kCh].volumes = (v << 16) + v;
}

int SfxIsPlaying(sfx_mixer* self, void* h) {
  int const kCh = FindChannel(self, h);
  return (kCh >= 0);
}

void SfxMixerStop(sfx_mixer* self, void* h) {
  int const kCh = FindChannel(self, h);
  if (kCh < 0) {
    return;
  }

  self->channel_states[kCh].flags = INACTIVE_FLAGS;
}

void* SfxMixerAdd(sfx_mixer* self, sfx_sound* snd, uint32_t time, void* h, uint32_t flags) {
  // A null sound is a disabled/placeholder slot. Silently ignore it so
  // callers that play by index don't need to special-case it.
  if (!snd) {
    return nullptr;
  }

  int const kChIdx = FindFreeChannel(self);

  if (kChIdx >= 0) {
    SDL_MemoryBarrierAcquire();
    channel* ch = self->channel_states + kChIdx;
    ch->sound = snd;
    ch->pos = time;
    ch->soundpos = 0;
    ch->stride = 0x100000000ULL;
    ch->volumes = 0x10001000;
    ch->id = h;

    SDL_MemoryBarrierRelease();  // Make sure everything is written before ch->flags

    ch->flags = flags;
    return h;
  }

  return nullptr;
}

#undef CHECK_HANDLE
