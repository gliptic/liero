#ifndef LIERO_SFX_HPP
#define LIERO_SFX_HPP

#include <SDL/SDL_mixer.h>
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
		
		int id; // ID of the sound playing on this channel
	};
	
	~Sfx();
	
	void init();
	void loadFromSND();
	
	void play(int sound, int id = -1, int loops = 0);	
	bool isPlaying(int id);
	void playOn(int channel, int sound, int id, int loops = 0);
	void stop(int id);
	
	std::vector<Mix_Chunk> sounds;
	ChannelInfo channelInfo[8];
};

extern Sfx sfx;

struct SoundPlayer : gvl::shared
{
	virtual void play(int sound, int id = -1, int loops = 0) = 0;
	virtual bool isPlaying(int id) = 0;
	virtual void playOn(int channel, int sound, int id, int loops = 0) = 0;
	virtual void stop(int id) = 0;
};

struct DefaultSoundPlayer : SoundPlayer
{
	void play(int sound, int id = -1, int loops = 0)
	{
		sfx.play(sound, id, loops);
	}
	
	bool isPlaying(int id)
	{
		return sfx.isPlaying(id);
	}
	
	void playOn(int channel, int sound, int id, int loops = 0)
	{
		return sfx.playOn(channel, sound, id, loops);
	}
	
	void stop(int id)
	{
		sfx.stop(id);
	}
};

struct NullSoundPlayer : SoundPlayer
{
	void play(int sound, int id, int loops)
	{
	}
	
	bool isPlaying(int id)
	{
	}
	
	void playOn(int channel, int sound, int id, int loops)
	{
	}
	
	void stop(int id)
	{
	}
};

#endif // LIERO_SFX_HPP
