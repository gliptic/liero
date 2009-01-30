#include "sfx.hpp"
#include "reader.hpp"
#include "console.hpp"
#include <vector>
#include <cassert>
#include <SDL/SDL.h>
#include <iostream>

#include <cmath> // TEMP

Sfx sfx;

void Sfx::init()
{
	int ret = Mix_OpenAudio(22050, AUDIO_S16SYS, 1, 512);
	Mix_AllocateChannels(8);
	Mix_Volume(-1, 128);
	
	assert(ret == 0);
}

void Sfx::loadFromSND()
{
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
		
		int byteLength = length * 2;
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
			fread(&temp[0], 1, length, snd);
		}
		
		for(int j = 0; j < length; ++j)
		{
			ptr[j] = int(temp[j]) * 30;
		}
		
		
	}
}

void Sfx::play(int sound, int id, int loops)
{
	for(int i = 0; i < 8; ++i)
	{
		if(!Mix_Playing(i))
		{
			playOn(i, sound, id, loops);
			return;
		}
	}
}

void Sfx::playOn(int channel, int sound, int id, int loops)
{
	if(sound < 0 || sound >= int(sounds.size()))
	{
		Console::writeWarning("Attempt to play non-existent sound");
		return;
	}
	Mix_PlayChannel(channel, &sounds[sound], loops);
	channelInfo[channel].id = id;
}

void Sfx::stop(int id)
{
	for(int i = 0; i < 8; ++i)
	{
		if(Mix_Playing(i) && channelInfo[i].id == id)
		{
			Mix_HaltChannel(i);
		}
	}
}

bool Sfx::isPlaying(int id)
{
	for(int i = 0; i < 8; ++i)
	{
		if(Mix_Playing(i) && channelInfo[i].id == id)
			return true;
	}
	
	return false;
}

Sfx::~Sfx()
{
	for(std::size_t i = 0; i < sounds.size(); ++i)
	{
		delete [] sounds[i].abuf;
		sounds[i].abuf = 0;
	}
}
