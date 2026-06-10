#include "player.hpp"
#include <string>
#include "../common.hpp"
#include "../console.hpp"

SoundPlayer* g_sound_player = nullptr;

void SoundPlayer::Play(SoundDefT hook, void* id, int loops) {
  Common const* c = GetCommonPtr();
  if (c) {
    Play(c->sound_hook[hook], id, loops);
  }
}

#if !DISABLE_SOUND
static void SDLCALL DefaultSoundPlayerStreamCallback(void* userdata, SDL_AudioStream* stream,
                                                     int additional_amount, int /*total_amount*/) {
  if (additional_amount > 0) {
    auto* data = SDL_stack_alloc(uint8_t, additional_amount);
    if (data) {
      uint32_t const kFrameCount = additional_amount / 2;
      SfxMixerMix(static_cast<sfx_mixer*>(userdata), data, kFrameCount);
      SDL_PutAudioStreamData(stream, data, additional_amount);
      SDL_stack_free(data);
    }
  }
}
#endif

DefaultSoundPlayer::DefaultSoundPlayer(Common& c) : m_common_(&c), mixer_(SfxMixerCreate()) {
#if !DISABLE_SOUND
  // Request a small audio buffer for low latency (~5.8ms at 44100Hz).
  // Must be set before opening the audio device.
  SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, "256");

  SDL_InitSubSystem(SDL_INIT_AUDIO);

  const SDL_AudioSpec kSpec = {.format = SDL_AUDIO_S16, .channels = 1, .freq = 44100};
  stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &kSpec,
                                      DefaultSoundPlayerStreamCallback, mixer_);

  if (stream_) {
    initialized_ = true;
    SDL_ResumeAudioStreamDevice(stream_);
  } else {
    console::WriteWarning(std::string("SDL_OpenAudioDeviceStream returned error: ") +
                          SDL_GetError());
  }
#endif
}

DefaultSoundPlayer::~DefaultSoundPlayer() {
#if !DISABLE_SOUND
  if (!initialized_) {
    return;
  }
  initialized_ = false;

  if (stream_) {
    SDL_DestroyAudioStream(stream_);
    stream_ = nullptr;
  }
  SDL_QuitSubSystem(SDL_INIT_AUDIO);
#endif
}

void DefaultSoundPlayer::PlayImpl(int sound, void* id, int loops) {
#if !DISABLE_SOUND
  if (!initialized_) {
    return;
  }

  SfxMixerAdd(mixer_, m_common_->sounds[sound].sound, SfxMixerNow(mixer_), id,
              loops ? SFX_SOUND_LOOP : SFX_SOUND_NORMAL);
#endif
}

bool DefaultSoundPlayer::IsPlaying(void* id) {
#if !DISABLE_SOUND
  if (!initialized_) {
    return false;
  }
  return SfxIsPlaying(mixer_, id) != 0;
#else
  return false;
#endif
}

void DefaultSoundPlayer::Stop(void* id) {
#if !DISABLE_SOUND
  if (!initialized_) {
    return;
  }
  SfxMixerStop(mixer_, id);
#endif
}

void RecordSoundPlayer::PlayImpl(int sound, void* id, int loops) {
  SfxMixerAdd(mixer, m_common_.sounds[sound].sound, SfxMixerNow(mixer), id,
              loops ? SFX_SOUND_LOOP : SFX_SOUND_NORMAL);
}
