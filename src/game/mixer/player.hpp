#pragma once

#include "mixer.hpp"

struct Common;

struct SoundPlayer
{
	virtual ~SoundPlayer() = default;
	virtual void play(int sound, void* id = 0, int loops = 0) = 0;
	virtual bool isPlaying(void* id) = 0;
	virtual void stop(void* id) = 0;
};

struct RecordSoundPlayer : SoundPlayer
{
	RecordSoundPlayer(Common& common, sfx_mixer* mixer)
	: mixer(mixer)
	, common(common)
	{
	}

	sfx_mixer* mixer;
	Common& common;

	void play(int sound, void* id = 0, int loops = 0);

	bool isPlaying(void* id)
	{
		return sfx_is_playing(mixer, id) != 0;
	}

	void stop(void* id)
	{
		sfx_mixer_stop(mixer, id);
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
