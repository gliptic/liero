#ifndef LIERO_SFX_HPP
#define LIERO_SFX_HPP

#if !DISABLE_SOUND
#include <SDL/SDL_mixer.h>
extern "C" {
#include "mixer/mixer.h"
}
#endif
#include <vector>
#include <gvl/resman/shared_ptr.hpp>



struct Sfx
{
	struct ChannelInfo
	{
		ChannelInfo()
		: id(0)
		{
		}
		
		void* id; // ID of the sound playing on this channel
	};
	
	Sfx()
	: initialized(false)
	{
	}
	~Sfx();
	
	void init();
	void deinit();
	void loadFromSND();
	
	void play(int sound, void* id = 0, int loops = 0);	
	bool isPlaying(void* id);
	//void playOn(int channel, int sound, void* id, int loops = 0);
	void stop(void* id);
#if !DISABLE_SOUND
	std::vector<sfx_sound*> sounds;
	ChannelInfo channelInfo[8];
#endif

	sfx_mixer* mixer;
	bool initialized;
};

extern Sfx sfx;

struct SoundPlayer : gvl::shared
{
	virtual void play(int sound, void* id = 0, int loops = 0) = 0;
	virtual bool isPlaying(void* id) = 0;
	virtual void stop(void* id) = 0;
};

struct RecordSoundPlayer : SoundPlayer
{
	RecordSoundPlayer(sfx_mixer* mixer)
	: mixer(mixer)
	{
	}

	sfx_mixer* mixer;

	void play(int sound, void* id = 0, int loops = 0)
	{
		sfx_mixer_add(mixer, sfx.sounds[sound], sfx_mixer_now(mixer), id, loops ? SFX_SOUND_LOOP : SFX_SOUND_NORMAL);
	}
	
	bool isPlaying(void* id)
	{
		return sfx_is_playing(mixer, id) != 0;
	}
	
	void stop(void* id)
	{
		sfx_mixer_stop(mixer, id);
	}
};

struct DefaultSoundPlayer : SoundPlayer
{
	void play(int sound, void* id = 0, int loops = 0)
	{
		sfx.play(sound, id, loops);
	}
	
	bool isPlaying(void* id)
	{
		return sfx.isPlaying(id);
	}
	
	void stop(void* id)
	{
		sfx.stop(id);
	}
};

struct NullSoundPlayer : SoundPlayer
{
	void play(int sound, void* id, int loops)
	{
	}
	
	bool isPlaying(void* id)
	{
		return false;
	}
	
	void stop(void* id)
	{
	}
};

#endif // LIERO_SFX_HPP
