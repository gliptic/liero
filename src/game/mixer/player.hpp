#pragma once

#include "../constants.hpp"
#include "mixer.hpp"

#if !DISABLE_SOUND
#include <SDL3/SDL.h>
#endif

struct Common;

struct SoundPlayer {
  virtual ~SoundPlayer() = default;

  void play(int sound, void* id = 0, int loops = 0) {
    if (speculative) return;
    if (sound >= 0) playImpl(sound, id, loops);
  }

  void play(SOUND_DEF_T hook, void* id = 0, int loops = 0);

  virtual bool isPlaying(void* id) = 0;
  virtual void stop(void* id) = 0;

  // When true, play()/stop() are suppressed but isPlaying() passes
  // through. Set during predicted/resim frames to avoid duplicate
  // sound emission.
  bool speculative = false;

 protected:
  virtual void playImpl(int sound, void* id, int loops) = 0;
  virtual Common* common() { return nullptr; }
};

struct DefaultSoundPlayer : SoundPlayer {
  DefaultSoundPlayer(Common& common);
  ~DefaultSoundPlayer();

  bool isPlaying(void* id);
  void stop(void* id);

  // Repoint at a new TC's Common. Called when the user switches TC at
  // runtime — without this the player keeps reading sound samples and
  // soundHook[] from the previous TC.
  void setCommon(Common& common) { m_common = &common; }

 protected:
  void playImpl(int sound, void* id, int loops);
  Common* common() { return m_common; }

 private:
  Common* m_common;
  sfx_mixer* mixer;
#if !DISABLE_SOUND
  SDL_AudioStream* stream;
#endif
  bool initialized;
};

struct RecordSoundPlayer : SoundPlayer {
  RecordSoundPlayer(Common& c, sfx_mixer* mixer) : mixer(mixer), m_common(c) {}

  sfx_mixer* mixer;

  bool isPlaying(void* id) { return sfx_is_playing(mixer, id) != 0; }

  void stop(void* id) {
    if (speculative) return;
    sfx_mixer_stop(mixer, id);
  }

 protected:
  void playImpl(int sound, void* id, int loops);
  Common* common() { return &m_common; }

 private:
  Common& m_common;
};

struct NullSoundPlayer : SoundPlayer {
  bool isPlaying(void* /*id*/) { return false; }
  void stop(void* /*id*/) {}

 protected:
  void playImpl(int /*sound*/, void* /*id*/, int /*loops*/) {}
};

extern SoundPlayer* g_soundPlayer;
