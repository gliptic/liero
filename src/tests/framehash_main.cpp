// Headless replay frame hasher: plays back a .lrp replay through the real
// game-draw path and prints one FNV-1a hash per frame of the composed RGB
// screen (palette-resolved). Two builds rendering the same replay must
// print identical streams for the renderer to count as pixel-identical;
// used to verify the modern-colors stage 2 refactor keeps classic output
// byte-for-byte stable.
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>

#include "game/common.hpp"
#include "game/constants.hpp"
#include "game/filesystem.hpp"
#include "game/game.hpp"
#include "game/gfx/renderer.hpp"
#include "game/io/stream.hpp"
#include "game/mixer/player.hpp"
#include "game/replay.hpp"
#include "game/spectatorviewport.hpp"
#include "game/viewport.hpp"

namespace {

constexpr uint64_t kFnvOffset = 1469598103934665603ULL;
constexpr uint64_t kFnvPrime = 1099511628211ULL;

uint64_t FnvByte(uint64_t h, uint8_t b) { return (h ^ b) * kFnvPrime; }

// The composition-time fade, identical to Palette::Fade's per-channel math.
uint8_t FadeChannel(uint8_t v, int amount) {
  return amount >= 32 ? v : static_cast<uint8_t>((v * amount) >> 5);
}

// Hashes the visible RGB output: the ARGB back buffer with the renderer's
// fade applied, exactly what composition puts on screen.
uint64_t HashFrame(Renderer& renderer) {
  int const kFade = renderer.fade_value;
  uint64_t h = kFnvOffset;
  for (int y = 0; y < renderer.bmp.h; ++y) {
    for (int x = 0; x < renderer.bmp.w; ++x) {
      uint32_t const kC = renderer.bmp.GetPixel(x, y);
      h = FnvByte(h, FadeChannel((kC >> 16) & 0xFF, kFade));
      h = FnvByte(h, FadeChannel((kC >> 8) & 0xFF, kFade));
      h = FnvByte(h, FadeChannel(kC & 0xFF, kFade));
    }
  }
  return h;
}

}  // namespace

int main(int argc, char* argv[]) try {
  if (argc < 3) {
    std::fprintf(stderr, "usage: framehash <tc-dir> <replay.lrp> [s|n] [rgb-dump-file]\n");
    return 2;
  }
  std::string const kTcDir = argv[1];
  std::string const kReplayPath = argv[2];
  bool const kSpectator = argc > 3 && argv[3][0] == 's';
  std::FILE* dump = argc > 4 ? std::fopen(argv[4], "wbe") : nullptr;

  PrecomputeTables();
  auto common = std::make_shared<Common>();
  common->load(FsNode(kTcDir));

  ReplayReader replay_reader(std::make_unique<io::FileReader>(kReplayPath.c_str(), "rb"));

  Renderer renderer;
  renderer.Init(kSpectator ? 640 : 320, kSpectator ? 400 : 200);
  renderer.LoadPalette(*common);

  auto sound = std::make_shared<NullSoundPlayer>();
  std::unique_ptr<Game> game = replay_reader.BeginPlayback(common, sound);
  if (!game) {
    std::fprintf(stderr, "BeginPlayback failed\n");
    return 1;
  }
  // BeginPlayback doesn't wire the game into the reader; ReplayController
  // normally does that (replayController.cpp).
  replay_reader.game = game.get();

  // Same viewport layout the video tool uses.
  game->ClearViewports();
  if (game->worms.size() >= 2) {
    game->worms[0]->stats_x = 0;
    game->worms[1]->stats_x = 218;
  }
  game->AddSpectatorViewport(new SpectatorViewport(Rect(0, 0, 504 + 68, 350), 504, 350));
  if (!game->worms.empty()) {
    game->AddViewport(new Viewport(Rect(0, 0, 158, 158), game->worms[0]->index, 504, 350));
  }
  if (game->worms.size() >= 2) {
    game->AddViewport(new Viewport(Rect(160, 0, 158 + 160, 158), game->worms[1]->index, 504, 350));
  }
  game->StartGame();
  game->Focus(renderer);

  int frame = 0;
  uint64_t all = kFnvOffset;
  while (replay_reader.PlaybackFrame(renderer)) {
    game->ProcessFrame();
    renderer.Clear();
    game->Draw(renderer, kStateGame, kSpectator, /*is_replay=*/true);

    // Hash with the fade that was in effect during the draw (0 on the very
    // first frame, like the video tool), then unfade for the rest.
    uint64_t const kH = HashFrame(renderer);
    all = (all ^ kH) * kFnvPrime;
    std::printf("%d %016" PRIx64 "\n", frame, kH);

    if (dump) {
      int const kFade = renderer.fade_value;
      for (int py = 0; py < renderer.bmp.h; ++py) {
        for (int px = 0; px < renderer.bmp.w; ++px) {
          uint32_t const kC = renderer.bmp.GetPixel(px, py);
          uint8_t const kRgb[3] = {FadeChannel((kC >> 16) & 0xFF, kFade),
                                   FadeChannel((kC >> 8) & 0xFF, kFade),
                                   FadeChannel(kC & 0xFF, kFade)};
          std::fwrite(kRgb, 1, 3, dump);
        }
      }
    }
    renderer.fade_value = 33;
    ++frame;
  }
  if (dump) {
    std::fclose(dump);  // NOLINT(cert-err33-c) — verification tool
  }
  std::printf("total %d %016" PRIx64 "\n", frame, all);
  return 0;
} catch (std::exception& ex) {
  std::fprintf(stderr, "EXCEPTION: %s\n", ex.what());
  return 1;
}
