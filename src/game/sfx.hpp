#pragma once

#if !DISABLE_SOUND
#include "mixer/mixer.hpp"
#include <SDL3/SDL.h>
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
	, stream(nullptr)
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
	SDL_AudioStream* stream;
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
