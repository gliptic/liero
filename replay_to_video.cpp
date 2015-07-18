#include "replay_to_video.hpp"

#include <string>
#include "replay.hpp"
#include "filesystem.hpp"
#include "reader.hpp"
#include "mixer/player.hpp"
#include "game.hpp"
#include "viewport.hpp"
#include "spectatorviewport.hpp"
#include "gfx/renderer.hpp"
#include "text.hpp"

#include <gvl/io2/fstream.hpp>
#include <memory>

extern "C"
{
#include "video_recorder.h"
#include "tl/vector.h"
#include "mixer/mixer.h"
}

void replayToVideo(
	gvl::shared_ptr<Common> const& common,
	std::string const& fullPath,
	std::string const& replayVideoName)
{
	auto replay(
		gvl::to_source(new gvl::file_bucket_pipe(fullPath.c_str(), "rb")));
	ReplayReader replayReader(replay);
	Renderer renderer, spectatorRenderer;

	renderer.init(320, 200);
	renderer.loadPalette(*common);
	spectatorRenderer.init(640, 400);
	spectatorRenderer.loadPalette(*common);

	sfx_mixer* mixer = sfx_mixer_create();

	std::unique_ptr<Game> game(
		replayReader.beginPlayback(common,
			gvl::shared_ptr<SoundPlayer>(new RecordSoundPlayer(*common, mixer))));

	// FIXME: the viewports are changed based on the replay for some
	// reason, so we need to restore them here. Probably makes more sense
	// to not save the viewports at all. But that probably breaks save
	// format compatibility?
	game->clearViewports();

	// for backwards compatibility reasons, this is not stored within the
	// replay. Yet.
	game->worms[0]->statsX = 0;
	game->worms[1]->statsX = 218;

	// spectator viewport is always full size
	// +68 on x to align the viewport in the middle
	game->addSpectatorViewport(new SpectatorViewport(gvl::rect(0, 0, 504 + 68, 350), 504, 350));
	game->addViewport(new Viewport(gvl::rect(0, 0, 158, 158), game->worms[0]->index, 504, 350));
	game->addViewport(new Viewport(gvl::rect(160, 0, 158+160, 158), game->worms[1]->index, 504, 350));
	game->startGame();
	game->focus(renderer);
	game->focus(spectatorRenderer);

	int w = 1280, h = 720;

	AVRational framerate;
	framerate.num = 1;
	framerate.den = 30;

	AVRational nativeFramerate;
	nativeFramerate.num = 1;
	nativeFramerate.den = 70;

	av_register_all();
	video_recorder vidrec;
	vidrec_init(&vidrec, replayVideoName.c_str(), w, h, framerate);

	tl_vector soundBuffer;
	tl_vector_new_empty(soundBuffer);

	std::size_t audioCodecFrames = 1024;

	AVRational sampleDebt;
	sampleDebt.num = 0;
	sampleDebt.den = 70;

	AVRational frameDebt;
	frameDebt.num = 0;
	frameDebt.den = 1;

	int offsetX, offsetY;
	int mag = fitScreen(w, h, renderer.bmp.w, renderer.bmp.h, offsetX, offsetY);

	printf("\n");

	int f = 0;

    while(replayReader.playbackFrame(renderer))
	{
		game->processFrame();
		// because of bugs in spectatorviewport and its use of randomness,
		// we must draw twice
		renderer.clear();
		game->draw(renderer, false, true);
		spectatorRenderer.clear();
		game->draw(spectatorRenderer, true, true);
		++f;
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

				Color realPal[256];
				renderer.pal.activate(realPal);
				PalIdx* src = renderer.bmp.pixels;
				std::size_t destPitch = vidrec.tmp_picture->linesize[0];
				uint8_t* dest = vidrec.tmp_picture->data[0] + offsetY * destPitch + offsetX * 4;
				std::size_t srcPitch = renderer.bmp.pitch;

				uint32_t pal32[256];
				preparePaletteBgra(realPal, pal32);

				scaleDraw(src, renderer.renderResX, renderer.renderResY, srcPitch, dest, destPitch, mag, pal32);

				vidrec_write_video_frame(&vidrec, vidrec.tmp_picture);
			}

			// Move rest to the beginning of the buffer
			assert(audioSamples + samplesLeft == tl_vector_idx(soundBuffer, int16_t, soundBuffer.size));
			memmove(soundBuffer.impl, audioSamples, samplesLeft * sizeof(int16_t));
			soundBuffer.size = samplesLeft;
		}

		if ((f % (70 * 5)) == 0)
		{
			printf("\n%s", timeToStringFrames(f));
		}
	}

	tl_vector_free(soundBuffer);
	vidrec_finalize(&vidrec);
}
