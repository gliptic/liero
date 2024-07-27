#include "player.hpp"
#include "../common.hpp"

void RecordSoundPlayer::play(int sound, void* id, int loops)
{
	sfx_sound* sample = common.soundSample(sound);
	if (sample != nullptr)
		sfx_mixer_add(mixer, sample, sfx_mixer_now(mixer), id, loops ? SFX_SOUND_LOOP : SFX_SOUND_NORMAL);
}