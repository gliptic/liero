#pragma once

#include "../constants.hpp"
#include "mixer.hpp"

#if !DISABLE_SOUND
#include <SDL3/SDL.h>
#endif

struct Common;

struct SoundPlayer {
  virtual ~SoundPlayer() = default;

  void Play(int sound, void* id = 0, int loops = 0) {
    if (speculative) return;
    if (sound >= 0) PlayImpl(sound, id, loops);
  }

  void Play(SoundDefT hook, void* id = 0, int loops = 0);

  virtual bool IsPlaying(void* id) = 0;
  virtual void Stop(void* id) = 0;

  // When true, play()/stop() are suppressed but isPlaying() passes
  // through. Set during predicted/resim frames to avoid duplicate
  // sound emission.
  bool speculative = false;

 protected:
  virtual void PlayImpl(int sound, void* id, int loops) = 0;
  // Returns the active TC's Common bundle, or nullptr if unavailable.
  // Named GetCommonPtr (not Common) to avoid colliding with the
  // struct name `Common` after Google-style renaming.
  virtual Common* GetCommonPtr() { return nullptr; }
};

struct DefaultSoundPlayer : SoundPlayer {
  DefaultSoundPlayer(Common& common);
  ~DefaultSoundPlayer();

  bool IsPlaying(void* id);
  void Stop(void* id);

  // Repoint at a new TC's Common. Called when the user switches TC at
  // runtime — without this the player keeps reading sound samples and
  // soundHook[] from the previous TC.
  void SetCommon(Common& common) { m_common_ = &common; }

 protected:
  void PlayImpl(int sound, void* id, int loops);
  Common* GetCommonPtr() { return m_common_; }

 private:
  Common* m_common_;
  sfx_mixer* mixer_;
#if !DISABLE_SOUND
  SDL_AudioStream* stream_;
#endif
  bool initialized_;
};

struct RecordSoundPlayer : SoundPlayer {
  RecordSoundPlayer(Common& c, sfx_mixer* mixer) : mixer(mixer), m_common_(c) {}

  sfx_mixer* mixer;

  bool IsPlaying(void* id) { return SfxIsPlaying(mixer, id) != 0; }

  void Stop(void* id) {
    if (speculative) return;
    SfxMixerStop(mixer, id);
  }

 protected:
  void PlayImpl(int sound, void* id, int loops);
  Common* GetCommonPtr() { return &m_common_; }

 private:
  Common& m_common_;
};

struct NullSoundPlayer : SoundPlayer {
  bool IsPlaying(void* /*id*/) { return false; }
  void Stop(void* /*id*/) {}

 protected:
  void PlayImpl(int /*sound*/, void* /*id*/, int /*loops*/) {}
};

extern SoundPlayer* g_sound_player;
