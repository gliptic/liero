#include "replay_to_video.hpp"

#include <cstdio>
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

void ReplayToVideo(std::shared_ptr<Common> const& common, bool spectator,
                   std::string const& full_path, std::string const& replay_video_name) {
  ReplayReader replay_reader(std::make_unique<io::FileReader>(full_path.c_str(), "rb"));
  Renderer renderer;

  if (spectator) {
    renderer.Init(640, 400);
    renderer.LoadPalette(*common);
  } else {
    renderer.Init(320, 200);
    renderer.LoadPalette(*common);
  }

  sfx_mixer* mixer = SfxMixerCreate();

  std::unique_ptr<Game> game(replay_reader.BeginPlayback(
      common, std::shared_ptr<SoundPlayer>(new RecordSoundPlayer(*common, mixer))));

  // BeginPlayback doesn't wire the game into the reader; ReplayController
  // normally does that (replayController.cpp).
  replay_reader.game = game.get();

  // FIXME: the viewports are changed based on the replay for some
  // reason, so we need to restore them here. Probably makes more sense
  // to not save the viewports at all. But that probably breaks save
  // format compatibility?
  game->ClearViewports();

  // for backwards compatibility reasons, this is not stored within the
  // replay. Yet.
  if (game->worms.size() >= 2) {
    game->worms[0]->stats_x = 0;
    game->worms[1]->stats_x = 218;
  }

  // spectator viewport is always full size
  // +68 on x to align the viewport in the middle
  game->AddSpectatorViewport(new SpectatorViewport(Rect(0, 0, 504 + 68, 350)));
  game->AddViewport(new Viewport(Rect(0, 0, 158, 158), game->worms[0]->index));
  game->AddViewport(new Viewport(Rect(160, 0, 158 + 160, 158), game->worms[1]->index));
  game->StartGame();
  game->Focus(renderer);

  int const kW = 1280;
  int const kH = 720;

  AVRational framerate;
  framerate.num = 1;
  framerate.den = 60;

  AVRational native_framerate;
  native_framerate.num = 1;
  native_framerate.den = 70;

  video_recorder vidrec;
  VidrecInit(&vidrec, replay_video_name.c_str(), kW, kH, framerate);

  std::vector<int16_t> sound_buffer = std::vector<int16_t>();

  std::size_t const kAudioCodecFrames = 1024;

  AVRational sample_debt;
  sample_debt.num = 0;
  sample_debt.den = 70;

  AVRational frame_debt;
  frame_debt.num = 0;
  frame_debt.den = 1;

  int offset_x = 0;
  int offset_y = 0;
  int const kMag = FitScreen(640, 400, renderer.bmp.w, renderer.bmp.h, offset_x, offset_y);

  int f = 0;

  while (replay_reader.PlaybackFrame(renderer)) {
    game->ProcessFrame();
    renderer.Clear();
    game->Draw(renderer, kStateGame, spectator, /*is_replay=*/true);
    ++f;

    sample_debt.num += 44100;                                    // sampleDebt += 44100 / 70
    int const kMixerFrames = sample_debt.num / sample_debt.den;  // floor(sampleDebt)
    sample_debt.num -= kMixerFrames * sample_debt.den;           // sampleDebt -= mixerFrames

    std::size_t const kMixerStart = sound_buffer.size();
    sound_buffer.resize(kMixerStart + kMixerFrames);

    SfxMixerMix(mixer, &sound_buffer[kMixerStart], kMixerFrames);

    {
      int16_t* audio_samples = sound_buffer.data();
      std::size_t samples_left = sound_buffer.size();

      while (samples_left > kAudioCodecFrames) {
        VidrecWriteAudioFrame(&vidrec, audio_samples, kAudioCodecFrames);
        audio_samples += kAudioCodecFrames;
        samples_left -= kAudioCodecFrames;
      }

      frame_debt = av_add_q(frame_debt, native_framerate);

      if (av_cmp_q(frame_debt, framerate) > 0) {
        frame_debt = av_sub_q(frame_debt, framerate);

        std::size_t const kDestPitch = vidrec.tmp_picture->linesize[0];
        uint8_t* dest = vidrec.tmp_picture->data[0] + offset_y * kDestPitch + offset_x * 4;
        std::size_t const kSrcPitch = renderer.bmp.pitch;

        // The back buffer is already ARGB (BGRA byte order, as the encoder
        // expects); composition applies the renderer fade.
        ScaleDraw(renderer.bmp.pixels, renderer.render_res_x, renderer.render_res_y, kSrcPitch,
                  dest, kDestPitch, kMag, renderer.fade_value);

        VidrecWriteVideoFrame(&vidrec, vidrec.tmp_picture);
      }

      // Composition above uses the fade in effect during the draw (0 on
      // the very first frame, faded-out black like the old palette path).
      renderer.fade_value = 33;

      // Move remaining samples to the beginning of the buffer
      std::size_t const kOffset = audio_samples - sound_buffer.data();
      sound_buffer.erase(sound_buffer.begin(), sound_buffer.begin() + kOffset);
    }

    if ((f % (70 * 5)) == 0) {
      std::printf("\r%s", TimeToStringFrames(f));
      fflush(stdout);  // NOLINT(cert-err33-c) — progress indicator on stdout; failures are
                       // non-fatal here.
    }
  }

  VidrecFinalize(&vidrec);
}
