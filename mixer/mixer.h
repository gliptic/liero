#ifndef UUID_3B31F81A4DBD4530E2E6F5ACC61DDDAB
#define UUID_3B31F81A4DBD4530E2E6F5ACC61DDDAB

#include "tl/cstdint.h"
//#include "sfx.h"

#define CHANNEL_COUNT (8)

typedef struct sfx_mixer sfx_mixer;

typedef struct sfx_sound sfx_sound;

sfx_mixer* sfx_mixer_create(void);
void sfx_set_volume(sfx_mixer* self, uint32_t h, double speed);
uint32_t sfx_mixer_add(sfx_mixer* self, sfx_sound* snd, uint32_t time);

void sfx_mixer_mix(sfx_mixer* self, void* output, unsigned long frame_count, uint32_t now);
//void sfx_mixer_fill(sfx_stream* str, uint32_t start, uint32_t frames);

#endif // UUID_3B31F81A4DBD4530E2E6F5ACC61DDDAB
