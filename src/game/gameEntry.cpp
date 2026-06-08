#include <SDL3/SDL.h>

#include "console.hpp"
#include "constants.hpp"
#include "filesystem.hpp"
#include "game.hpp"
#include "gfx.hpp"
#include "keys.hpp"
#include "math.hpp"
#include "mixer/player.hpp"
#include "reader.hpp"
#include "text.hpp"
#include "viewport.hpp"
#include "worm.hpp"

#include <cstdlib>
#include <ctime>
#include <exception>
#include <memory>

int GameEntry(int argc, char* argv[]) try {
  // TODO: Better PRNG seeding
  gfx.rand.Seed(static_cast<uint32_t>(std::time(nullptr)));

#if OPENLIERO_EMSCRIPTEN
  // Emscripten preloads all data under /openliero; use single-dir mode.
  const char* emscriptenArgv[] = {argv[0], "--config-root", "/openliero", nullptr};
  auto r = paths::Resolve(3, const_cast<char**>(emscriptenArgv));
#else
  auto r = paths::Resolve(argc, argv);
#endif

  gfx.online_port = r.port != 0 ? r.port : gfx.online_port;
  gfx.SetConfigNodes(r.config_node, r.user_config_node);

  std::string tc_name;
  bool const kTcSet = !r.positional_args.empty();
  if (kTcSet) tc_name = r.positional_args[0];

  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD);

  InitKeys();

  PrecomputeTables();

  gfx.LoadMenus();

  gfx.Init();

  FsNode const kConfigNode(gfx.GetConfigNode());
  FsNode const kUserConfigNode(gfx.GetUserConfigNode());

  if (!gfx.LoadSettings(kConfigNode / "Setups" / "liero.cfg")) {
    gfx.settings = std::make_shared<Settings>();
    gfx.SaveSettings(kUserConfigNode / "Setups" / "liero.cfg");
  }

  if (kTcSet) gfx.settings->tc = tc_name;

  // TC loading
  FsNode const kLieroRoot(kConfigNode / "TC" / gfx.settings->tc);
  std::shared_ptr<Common> const kCommon = std::make_shared<Common>();
  kCommon->load(kLieroRoot);
  gfx.common = kCommon;
  gfx.play_renderer.LoadPalette(*kCommon);

  gfx.SetVideoMode();
  gfx.sound_player = std::make_shared<DefaultSoundPlayer>(*kCommon);
  g_sound_player = gfx.sound_player.get();

  gfx.MainLoop();

  gfx.settings->save(kUserConfigNode / "Setups" / "liero.cfg", gfx.rand);

  g_sound_player = nullptr;
  gfx.sound_player.reset();
  SDL_Quit();

  return 0;
} catch (std::exception&) {
  SDL_Quit();
  throw;
}
