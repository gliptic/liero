#ifndef LIERO_SFX_HPP
#define LIERO_SFX_HPP

#if !DISABLE_SOUND
extern "C" {
#include "mixer/mixer.h"
}
#endif
#include <vector>
#include "mixer/player.hpp"

struct Common;

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
	
	void play(Common& common, int sound, void* id = 0, int loops = 0);	
	bool isPlaying(void* id);

	void stop(void* id);
#if !DISABLE_SOUND
	ChannelInfo channelInfo[8];
#endif

	sfx_mixer* mixer;
	bool initialized;
};

extern Sfx sfx;


struct DefaultSoundPlayer : SoundPlayer
{
	DefaultSoundPlayer(Common& common)
	: common(common)
	{
	}

	Common& common;

	void play(int sound, void* id = 0, int loops = 0)
	{
		sfx.play(common, sound, id, loops);
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



#endif // LIERO_SFX_HPP
