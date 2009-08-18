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
		
		void* id; // ID of the sound playing on this channel
	};
	
	~Sfx();
	
	void init();
	void loadFromSND();
	
	void play(int sound, void* id = 0, int loops = 0);	
	bool isPlaying(void* id);
	void playOn(int channel, int sound, void* id, int loops = 0);
	void stop(void* id);
	
	std::vector<Mix_Chunk> sounds;
	ChannelInfo channelInfo[8];
};

extern Sfx sfx;

struct SoundPlayer : gvl::shared
{
	virtual void play(int sound, void* id = 0, int loops = 0) = 0;
	virtual bool isPlaying(void* id) = 0;
	virtual void playOn(int channel, int sound, void* id, int loops = 0) = 0;
	virtual void stop(void* id) = 0;
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
	
	void playOn(int channel, int sound, void* id, int loops = 0)
	{
		return sfx.playOn(channel, sound, id, loops);
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
	
	void playOn(int channel, int sound, void* id, int loops)
	{
	}
	
	void stop(void* id)
	{
	}
};

#endif // LIERO_SFX_HPP
