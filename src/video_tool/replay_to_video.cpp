#include "replay_to_video.hpp"

#include <string>
#include "game/filesystem.hpp"
#include "game/game.hpp"
#include "game/gfx/renderer.hpp"
#include "game/io/stream.hpp"
#include "game/mixer/player.hpp"
#include "game/reader.hpp"
#include "game/replay.hpp"
#include "game/spectatorviewport.hpp"
#include "game/text.hpp"
#include "game/viewport.hpp"

#include <memory>

extern "C" {
#include "video_recorder.h"
}
#include "game/mixer/mixer.hpp"

void replayToVideo(std::shared_ptr<Common> const& common, bool spectator,
                   std::string const& fullPath, std::string const& replayVideoName) {
  ReplayReader replayReader(std::make_unique<io::FileReader>(fullPath.c_str(), "rb"));
  Renderer renderer;

  if (spectator) {
    renderer.Init(640, 400);
    renderer.LoadPalette(*common);
  } else {
    renderer.Init(320, 200);
    renderer.LoadPalette(*common);
  }

  sfx_mixer* mixer = SfxMixerCreate();

  std::unique_ptr<Game> game(replayReader.BeginPlayback(
      common, std::shared_ptr<SoundPlayer>(new RecordSoundPlayer(*common, mixer))));

  // FIXME: the viewports are changed based on the replay for some
  // reason, so we need to restore them here. Probably makes more sense
  // to not save the viewports at all. But that probably breaks save
  // format compatibility?
  game->ClearViewports();

  // for backwards compatibility reasons, this is not stored within the
  // replay. Yet.
  game->worms[0]->stats_x = 0;
  game->worms[1]->stats_x = 218;

  // spectator viewport is always full size
  // +68 on x to align the viewport in the middle
  game->AddSpectatorViewport(new SpectatorViewport(Rect(0, 0, 504 + 68, 350), 504, 350));
  game->AddViewport(new Viewport(Rect(0, 0, 158, 158), game->worms[0]->index, 504, 350));
  game->AddViewport(new Viewport(Rect(160, 0, 158 + 160, 158), game->worms[1]->index, 504, 350));
  game->StartGame();
  game->Focus(renderer);

  int w = 1280, h = 720;

  AVRational framerate;
  framerate.num = 1;
  framerate.den = 60;

  AVRational nativeFramerate;
  nativeFramerate.num = 1;
  nativeFramerate.den = 70;

  video_recorder vidrec;
  vidrec_init(&vidrec, replayVideoName.c_str(), w, h, framerate);

  std::vector<int16_t> soundBuffer = std::vector<int16_t>();

  std::size_t audioCodecFrames = 1024;

  AVRational sampleDebt;
  sampleDebt.num = 0;
  sampleDebt.den = 70;

  AVRational frameDebt;
  frameDebt.num = 0;
  frameDebt.den = 1;

  int offsetX, offsetY;
  int mag = FitScreen(640, 400, renderer.bmp.w, renderer.bmp.h, offsetX, offsetY);

  int f = 0;

  while (replayReader.PlaybackFrame(renderer)) {
    game->ProcessFrame();
    renderer.Clear();
    game->Draw(renderer, kStateGame, spectator, true);
    ++f;
    renderer.fade_value = 33;

    sampleDebt.num += 44100;                            // sampleDebt += 44100 / 70
    int mixerFrames = sampleDebt.num / sampleDebt.den;  // floor(sampleDebt)
    sampleDebt.num -= mixerFrames * sampleDebt.den;     // sampleDebt -= mixerFrames

    std::size_t mixerStart = soundBuffer.size();
    soundBuffer.resize(mixerStart + mixerFrames);

    SfxMixerMix(mixer, &soundBuffer[mixerStart], mixerFrames);

    {
      int16_t* audioSamples = &soundBuffer[0];
      std::size_t samplesLeft = soundBuffer.size();

      while (samplesLeft > audioCodecFrames) {
        vidrec_write_audio_frame(&vidrec, audioSamples, audioCodecFrames);
        audioSamples += audioCodecFrames;
        samplesLeft -= audioCodecFrames;
      }

      frameDebt = av_add_q(frameDebt, nativeFramerate);

      if (av_cmp_q(frameDebt, framerate) > 0) {
        frameDebt = av_sub_q(frameDebt, framerate);

        Color realPal[256];
        renderer.pal.Activate(realPal);
        PalIdx* src = renderer.bmp.pixels;
        std::size_t destPitch = vidrec.tmp_picture->linesize[0];
        uint8_t* dest = vidrec.tmp_picture->data[0] + offsetY * destPitch + offsetX * 4;
        std::size_t srcPitch = renderer.bmp.pitch;

        uint32_t pal32[256];
        PreparePaletteBgra(realPal, pal32);

        ScaleDraw(src, renderer.render_res_x, renderer.render_res_y, srcPitch, dest, destPitch, mag,
                  pal32);

        vidrec_write_video_frame(&vidrec, vidrec.tmp_picture);
      }

      // Move remaining samples to the beginning of the buffer
      std::size_t offset = audioSamples - &soundBuffer[0];
      soundBuffer.erase(soundBuffer.begin(), soundBuffer.begin() + offset);
    }

    if ((f % (70 * 5)) == 0) {
      printf("\r%s", TimeToStringFrames(f));
      fflush(stdout);
    }
  }

  vidrec_finalize(&vidrec);
}
