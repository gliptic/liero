#include "player.hpp"
#include "../common.hpp"
#include "../console.hpp"
#include <string>

SoundPlayer* g_soundPlayer = nullptr;

void SoundPlayer::play(SOUND_DEF_T hook, void* id, int loops)
{
	Common* c = common();
	if (c)
		play(c->soundHook[hook], id, loops);
}

#if !DISABLE_SOUND
static void SDLCALL DefaultSoundPlayer_stream_callback(
	void* userdata, SDL_AudioStream* stream, int additional_amount, int /*total_amount*/)
{
	if (additional_amount > 0)
	{
		uint8_t* data = (uint8_t*)SDL_stack_alloc(uint8_t, additional_amount);
		if (data)
		{
			uint32_t frame_count = additional_amount / 2;
			sfx_mixer_mix((sfx_mixer*)userdata, data, frame_count);
			SDL_PutAudioStreamData(stream, data, additional_amount);
			SDL_stack_free(data);
		}
	}
}
#endif

DefaultSoundPlayer::DefaultSoundPlayer(Common& c)
: m_common(&c)
, mixer(nullptr)
#if !DISABLE_SOUND
, stream(nullptr)
#endif
, initialized(false)
{
#if !DISABLE_SOUND
	// Request a small audio buffer for low latency (~5.8ms at 44100Hz).
	// Must be set before opening the audio device.
	SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, "256");

	SDL_InitSubSystem(SDL_INIT_AUDIO);

	mixer = sfx_mixer_create();

	const SDL_AudioSpec spec = { SDL_AUDIO_S16, 1, 44100 };
	stream = SDL_OpenAudioDeviceStream(
		SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, DefaultSoundPlayer_stream_callback, mixer);

	if (stream)
	{
		initialized = true;
		SDL_ResumeAudioStreamDevice(stream);
	}
	else
	{
		Console::writeWarning(
			std::string("SDL_OpenAudioDeviceStream returned error: ") + SDL_GetError());
	}
#endif
}

DefaultSoundPlayer::~DefaultSoundPlayer()
{
#if !DISABLE_SOUND
	if (!initialized)
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

void DefaultSoundPlayer::playImpl(int sound, void* id, int loops)
{
#if !DISABLE_SOUND
	if (!initialized)
		return;

	sfx_mixer_add(
		mixer,
		m_common->sounds[sound].sound,
		sfx_mixer_now(mixer),
		id,
		loops ? SFX_SOUND_LOOP : SFX_SOUND_NORMAL);
#endif
}

bool DefaultSoundPlayer::isPlaying(void* id)
{
#if !DISABLE_SOUND
	if (!initialized)
		return false;
	return sfx_is_playing(mixer, id) != 0;
#else
	return false;
#endif
}

void DefaultSoundPlayer::stop(void* id)
{
#if !DISABLE_SOUND
	if (speculative) return;
	if (!initialized)
		return;
	sfx_mixer_stop(mixer, id);
#endif
}

void RecordSoundPlayer::playImpl(int sound, void* id, int loops)
{
	sfx_mixer_add(
		mixer,
		m_common.sounds[sound].sound,
		sfx_mixer_now(mixer),
		id,
		loops ? SFX_SOUND_LOOP : SFX_SOUND_NORMAL);
}
