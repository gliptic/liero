#include "replay_to_video.hpp"

#include <string>
#include "replay.hpp"
#include "filesystem.hpp"
#include "reader.hpp"
#include "sfx.hpp"
#include "game.hpp"
#include "gfx.hpp"

#include <gvl/io/fstream.hpp>
#include <memory>

extern "C"
{
#include "video_recorder.h"
#include "tl/vector.h"
#include "mixer/mixer.h"
}

void replayToVideo(
	Gfx& gfx,
	gvl::shared_ptr<Common> const& common,
	std::string const& fullPath,
	std::string const& replayVideoName)
{
	gvl::stream_ptr replay(new gvl::fstream(std::fopen(fullPath.c_str(), "rb")));
	ReplayReader replayReader(replay);
	Renderer renderer;

	renderer.init();

	std::string fullVideoPath = joinPath(lieroEXERoot, replayVideoName);

	std::auto_ptr<Game> game(replayReader.beginPlayback(common));

	sfx_mixer* mixer = sfx_mixer_create();

	game->soundPlayer.reset(new RecordSoundPlayer(mixer));
	game->startGame();
	game->focus();

	int w = 1280, h = 720;

	AVRational framerate;
	framerate.num = 1;
	framerate.den = 30;

	AVRational nativeFramerate;
	nativeFramerate.num = 1;
	nativeFramerate.den = 70;

	av_register_all();
	video_recorder vidrec;
	vidrec_init(&vidrec, fullVideoPath.c_str(), w, h, framerate);

	tl_vector soundBuffer;
	tl_vector_new_empty(soundBuffer);

	std::size_t audioCodecFrames = 1024;

	AVRational sampleDebt;
	sampleDebt.num = 0;
	sampleDebt.den = 70;

	AVRational frameDebt;
	frameDebt.num = 0;
	frameDebt.den = 1;

	uint32_t scaleFilter = Settings::SfNearest;

	int offsetX, offsetY;
	int mag = Gfx::fitScreen(w, h, gfx.screenBmp.w, gfx.screenBmp.h, offsetX, offsetY, scaleFilter);

	uint32_t prevShowFrameMs = SDL_GetTicks();
	int f = 0;
	while(replayReader.playbackFrame())
	{
		game->processFrame();
		renderer.clear();
		game->draw(renderer);
		renderer.fadeValue = 33;

		sampleDebt.num += 44100; // sampleDebt += 44100 / 70
		int mixerFrames = sampleDebt.num / sampleDebt.den; // floor(sampleDebt)
		sampleDebt.num -= mixerFrames * sampleDebt.den; // sampleDebt -= mixerFrames

		std::size_t mixerStart = soundBuffer.size;
		tl_vector_reserve(soundBuffer, int16_t, soundBuffer.size + mixerFrames);
		sfx_mixer_mix(mixer, tl_vector_idx(soundBuffer, int16_t, mixerStart), mixerFrames);
		tl_vector_post_enlarge(soundBuffer, int16_t, mixerFrames);
						
		{
			int16_t* audioSamples = tl_vector_idx(soundBuffer, int16_t, 0);
			std::size_t samplesLeft = soundBuffer.size;

			while (samplesLeft > audioCodecFrames)
			{
				vidrec_write_audio_frame(&vidrec, audioSamples, audioCodecFrames);
				audioSamples += audioCodecFrames;
				samplesLeft -= audioCodecFrames;
			}
								
			frameDebt = av_add_q(frameDebt, nativeFramerate);

			if (av_cmp_q(frameDebt, framerate) > 0)
			{
				frameDebt = av_sub_q(frameDebt, framerate);

				SDL_Color realPal[256];
				renderer.pal.activate(realPal);
				PalIdx* src = renderer.screenBmp.pixels;
				std::size_t destPitch = vidrec.tmp_picture->linesize[0];
				uint8_t* dest = vidrec.tmp_picture->data[0] + offsetY * destPitch + offsetX * 4;
				std::size_t srcPitch = renderer.screenBmp.pitch;
								
				uint32_t pal32[256];
				Gfx::preparePaletteBgra(realPal, pal32);

				Gfx::scaleDraw(src, 320, 200, srcPitch, dest, destPitch, mag, scaleFilter, pal32);

				vidrec_write_video_frame(&vidrec, vidrec.tmp_picture);
			}

			// Move rest to the beginning of the buffer
			assert(audioSamples + samplesLeft == tl_vector_idx(soundBuffer, int16_t, soundBuffer.size));
			memmove(soundBuffer.impl, audioSamples, samplesLeft * sizeof(int16_t));
			soundBuffer.size = samplesLeft;
		}

#if 0
		uint32_t nowMs = SDL_GetTicks();
		if (prevShowFrameMs + 100 < nowMs)
		{
			prevShowFrameMs = SDL_GetTicks();
			gfx.flip();
			gfx.process(0);
		}
#endif
	}

	tl_vector_free(soundBuffer);
	vidrec_finalize(&vidrec);
}