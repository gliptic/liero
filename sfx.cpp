#include "sfx.hpp"
#include "reader.hpp"
#include "console.hpp"
#include <vector>
#include <cassert>
#if !DISABLE_SOUND
#include <SDL/SDL.h>
#endif
#include <iostream>

#include <cmath> // TEMP

Sfx sfx;

#define HQ_SOUND 1

void Sfx::init()
{
#if !DISABLE_SOUND
	if(initialized)
		return;
	
	SDL_InitSubSystem(SDL_INIT_AUDIO);

#if HQ_SOUND
	int ret = Mix_OpenAudio(44100, AUDIO_S16SYS, 1, 4*512);
#else
	int ret = Mix_OpenAudio(22050, AUDIO_S16SYS, 1, 512);
#endif
	if(ret == 0)
	{
		initialized = true;
		Mix_AllocateChannels(8);
		Mix_Volume(-1, 128);
	}
	else
	{
		Console::writeWarning(std::string("Mix_OpenAudio returned error: ") + Mix_GetError());
	}
#endif
}

void Sfx::deinit()
{
#if !DISABLE_SOUND
	if(!initialized)
		return;
	initialized = false;

	Mix_CloseAudio();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
#endif
}

void Sfx::loadFromSND()
{
#if !DISABLE_SOUND
	FILE* snd = openLieroSND();
		
	int count = readUint16(snd);
	
	sounds.resize(count);
	
	long oldPos = ftell(snd);
	
	for(int i = 0; i < count; ++i)
	{
		fseek(snd, oldPos + 8, SEEK_SET); // Skip name
		
		int offset = readUint32(snd);
		int length = readUint32(snd);
		
		oldPos = ftell(snd);
		
#if HQ_SOUND
		int byteLength = length * 4;
#else
		int byteLength = length * 2;
#endif
		Uint8* buf = new Uint8[byteLength];
		
		sounds[i].allocated = 0;
		sounds[i].abuf = buf;
		sounds[i].alen = byteLength;
		sounds[i].volume = 128;
		
		Sint16* ptr = reinterpret_cast<Sint16*>(buf);
		
		std::vector<Sint8> temp(length);
		
		if(length > 0)
		{
			fseek(snd, offset, SEEK_SET);
			checkedFread(&temp[0], 1, length, snd);
		}
		
#if HQ_SOUND
		int prev = (temp[0]) * 30;
		*ptr++ = prev;
		
		for(int j = 1; j < length; ++j)
		{
			int cur = temp[j] * 30;
			*ptr++ = (prev + cur) / 2;
			*ptr++ = cur;
			prev = cur;
		}
		
		*ptr++ = prev;
#else
		for(int j = 0; j < length; ++j)
		{
			ptr[j] = int(temp[j]) * 30;
		}
#endif
		
	}
#endif
}

void Sfx::play(int sound, void* id, int loops)
{
#if !DISABLE_SOUND
	if(!initialized)
		return;

	for(int i = 0; i < 8; ++i)
	{
		if(!Mix_Playing(i))
		{
			playOn(i, sound, id, loops);
			return;
		}
	}
#endif
}

void Sfx::playOn(int channel, int sound, void* id, int loops)
{
#if !DISABLE_SOUND
	if(!initialized)
		return;
		
	if(sound < 0 || sound >= int(sounds.size()))
	{
		Console::writeWarning("Attempt to play non-existent sound");
		return;
	}
	Mix_PlayChannel(channel, &sounds[sound], loops);
	channelInfo[channel].id = id;
#endif
}

void Sfx::stop(void* id)
{
#if !DISABLE_SOUND
	if(!initialized)
		return;
		
	for(int i = 0; i < 8; ++i)
	{
		if(Mix_Playing(i) && channelInfo[i].id == id)
		{
			Mix_HaltChannel(i);
		}
	}
#endif
}

bool Sfx::isPlaying(void* id)
{
#if !DISABLE_SOUND
	if(!initialized)
		return false;
		
	for(int i = 0; i < 8; ++i)
	{
		if(Mix_Playing(i) && channelInfo[i].id == id)
			return true;
	}
#endif
	
	return false;
}

Sfx::~Sfx()
{
#if !DISABLE_SOUND
	for(std::size_t i = 0; i < sounds.size(); ++i)
	{
		delete [] sounds[i].abuf;
		sounds[i].abuf = 0;
	}
#endif
}
