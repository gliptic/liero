#include <SDL3/SDL.h>

#include "gfx.hpp"
#include "mixer/player.hpp"
#include "game.hpp"
#include "viewport.hpp"
#include "worm.hpp"
#include "reader.hpp"
#include "filesystem.hpp"
#include "text.hpp"
#include "keys.hpp"
#include "constants.hpp"
#include "math.hpp"
#include "console.hpp"

#include <ctime>
#include <cstdlib>
#include <exception>


int gameEntry(int argc, char* argv[])
try
{
	// TODO: Better PRNG seeding
	gfx.rand.seed(uint32_t(std::time(0)));

#if OPENLIERO_EMSCRIPTEN
	// Emscripten preloads all data under /openliero; use single-dir mode.
	const char* emscriptenArgv[] = { argv[0], "--config-root", "/openliero", nullptr };
	auto r = paths::resolve(3, const_cast<char**>(emscriptenArgv));
#else
	auto r = paths::resolve(argc, argv);
#endif

	gfx.onlinePort = r.port != 0 ? r.port : gfx.onlinePort;
	gfx.setConfigNodes(r.configNode, r.userConfigNode);

	std::string tcName;
	bool tcSet = !r.positionalArgs.empty();
	if (tcSet)
		tcName = r.positionalArgs[0];

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD);

	initKeys();

	precomputeTables();

	gfx.loadMenus();

	gfx.init();

	FsNode configNode(gfx.getConfigNode());
	FsNode userConfigNode(gfx.getUserConfigNode());

	if (!gfx.loadSettings(configNode / "Setups" / "liero.cfg"))
	{
		gfx.settings.reset(new Settings);
		gfx.saveSettings(userConfigNode / "Setups" / "liero.cfg");
	}

	if (tcSet)
		gfx.settings->tc = tcName;

	// TC loading
	FsNode lieroRoot(configNode / "TC" / gfx.settings->tc);
	std::shared_ptr<Common> common(new Common());
	common->load(std::move(lieroRoot));
	gfx.common = common;
	gfx.playRenderer.loadPalette(*common);

	gfx.setVideoMode();
	gfx.soundPlayer = std::make_shared<DefaultSoundPlayer>(*common);
	g_soundPlayer = gfx.soundPlayer.get();

	gfx.mainLoop();

	gfx.settings->save(userConfigNode / "Setups" / "liero.cfg", gfx.rand);

	g_soundPlayer = nullptr;
	gfx.soundPlayer.reset();
	SDL_Quit();

	return 0;
}
catch(std::exception&)
{
	SDL_Quit();
	throw;
}
