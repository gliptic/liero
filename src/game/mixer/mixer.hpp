#pragma once

#include <cstdint>
#include <vector>

#define CHANNEL_COUNT (8)

typedef struct sfx_mixer sfx_mixer;

typedef struct sfx_sound sfx_sound;

#define SFX_SOUND_NORMAL (0)
#define SFX_SOUND_LOOP (1)

sfx_mixer* sfx_mixer_create(void);
int32_t sfx_mixer_now(sfx_mixer* mixer);
void sfx_set_volume(sfx_mixer* self, void* h, double volume);
int sfx_is_playing(sfx_mixer* self, void* h);

void* sfx_mixer_add(sfx_mixer* self, sfx_sound* snd, uint32_t time, void* h, uint32_t flags);
void sfx_mixer_stop(sfx_mixer* self, void* h);

sfx_sound* sfx_new_sound(std::size_t samples);
void sfx_free_sound(sfx_sound* snd);
std::vector<int16_t>& sfx_sound_data(sfx_sound* snd);

void sfx_mixer_mix(sfx_mixer* self, void* output, unsigned long frame_count);
// void sfx_mixer_fill(sfx_stream* str, uint32_t start, uint32_t frames);
