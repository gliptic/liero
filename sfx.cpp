#include "sfx.hpp"
#include "reader.hpp"
#include "console.hpp"
#include "common.hpp"
#include <vector>
#include <cassert>
#if !DISABLE_SOUND
#include <SDL2/SDL.h>
#endif

Sfx sfx;

extern "C" void SDLCALL Sfx_callback(void *userdata, Uint8 *stream, int len)
{
	uint32 frame_count = len / 2;

	sfx_mixer_mix((sfx_mixer*)userdata, stream, frame_count);
}

void Sfx::init()
{
#if !DISABLE_SOUND
	if(initialized)
		return;

	SDL_InitSubSystem(SDL_INIT_AUDIO);

	mixer = sfx_mixer_create();

	SDL_AudioSpec spec;
	memset(&spec, 0, sizeof(spec));
	spec.channels = 1;
	spec.freq = 44100;
	spec.format = AUDIO_S16SYS;
	spec.size = 4*512;
	spec.callback = Sfx_callback;
	spec.userdata = mixer;

	int ret = SDL_OpenAudio(&spec, NULL);

	if(ret == 0)
	{
		initialized = true;
		SDL_PauseAudio(0);
	}
	else
	{
		Console::writeWarning(std::string("SDL_OpenAudio returned error: ") + SDL_GetError());
	}
#endif
}

void Sfx::deinit()
{
#if !DISABLE_SOUND
	if(!initialized)
		return;
	initialized = false;

	SDL_CloseAudio();
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
