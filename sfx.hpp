#ifndef LIERO_SFX_HPP
#define LIERO_SFX_HPP

#include <SDL/SDL_mixer.h>
#include <vector>

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

#endif // LIERO_SFX_HPP
