#pragma once

#include <cstdint>
#include <vector>

#define CHANNEL_COUNT (8)

typedef struct SfxMixer sfx_mixer;

typedef struct SfxSound sfx_sound;

#define SFX_SOUND_NORMAL (0)
#define SFX_SOUND_LOOP (1)

sfx_mixer* SfxMixerCreate(void);
int32_t SfxMixerNow(sfx_mixer* mixer);
void SfxSetVolume(sfx_mixer* self, void* h, double volume);
int SfxIsPlaying(sfx_mixer* self, void* h);

void* SfxMixerAdd(sfx_mixer* self, sfx_sound* snd, uint32_t time, void* h, uint32_t flags);
void SfxMixerStop(sfx_mixer* self, void* h);

sfx_sound* SfxNewSound(std::size_t samples);
void SfxFreeSound(sfx_sound* snd);
std::vector<int16_t>& SfxSoundData(sfx_sound* snd);

void SfxMixerMix(sfx_mixer* self, void* output, unsigned long frame_count);
// void sfx_mixer_fill(sfx_stream* str, uint32_t start, uint32_t frames);
