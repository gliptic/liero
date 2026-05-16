#include "sfx.hpp"
#include "reader.hpp"
#include "console.hpp"
#include "common.hpp"
#include <vector>
#include <cassert>
#if !DISABLE_SOUND
#	include <SDL3/SDL.h>
#endif

Sfx sfx;

static void SDLCALL Sfx_stream_callback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount)
{
	if (additional_amount > 0) {
		uint8_t *data = (uint8_t *)SDL_stack_alloc(uint8_t, additional_amount);
		if (data) {
			uint32_t frame_count = additional_amount / 2;
			sfx_mixer_mix((sfx_mixer*)userdata, data, frame_count);
			SDL_PutAudioStreamData(stream, data, additional_amount);
			SDL_stack_free(data);
		}
	}
}

void Sfx::init()
{
#if !DISABLE_SOUND
	if(initialized)
		return;

	// Request a small audio buffer for low latency (~5.8ms at 44100Hz).
	// Must be set before opening the audio device.
	SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, "256");

	SDL_InitSubSystem(SDL_INIT_AUDIO);

	mixer = sfx_mixer_create();

	const SDL_AudioSpec spec = { SDL_AUDIO_S16, 1, 44100 };
	stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, Sfx_stream_callback, mixer);

	if(stream)
	{
		initialized = true;
		SDL_ResumeAudioStreamDevice(stream);
	}
	else
	{
		Console::writeWarning(std::string("SDL_OpenAudioDeviceStream returned error: ") + SDL_GetError());
	}
#endif
}

void Sfx::deinit()
{
#if !DISABLE_SOUND
	if(!initialized)
		return;
	initialized = false;

	if (stream)
	{
		SDL_DestroyAudioStream(stream);
		stream = nullptr;
	}
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
#endif
}

void Sfx::play(Common& common, int sound, void* id, int loops)
{
#if !DISABLE_SOUND
	if(!initialized)
		return;

	sfx_mixer_add(mixer, common.sounds[sound].sound, sfx_mixer_now(mixer), id, loops ? SFX_SOUND_LOOP : SFX_SOUND_NORMAL);
#endif
}

void Sfx::stop(void* id)
{
#if !DISABLE_SOUND
	if(!initialized)
		return;

	sfx_mixer_stop(mixer, id);
#endif
}

bool Sfx::isPlaying(void* id)
{
#if !DISABLE_SOUND
	if(!initialized)
		return false;

	return sfx_is_playing(mixer, id) != 0;
#else
	return false;
#endif
}

Sfx::~Sfx()
{
	deinit();
}
