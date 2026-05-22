#include <SDL3/SDL.h>

#include "gfx.hpp"
#include "sfx.hpp"
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
#include <gvl/math/cmwc.hpp>

int gameEntry(int argc, char* argv[])
try
{
	// TODO: Better PRNG seeding
	gfx.rand.seed(uint32_t(std::time(0)));

	bool tcSet = false;

	std::string tcName;
#if OPENLIERO_EMSCRIPTEN
	std::string configPath = "/openliero";
#else
	std::string configPath; // Default to current dir
#endif

	for(int i = 1; i < argc; ++i)
	{
		if(argv[i][0] == '-')
		{
			switch(argv[i][1])
			{
			case '-':
				if (std::strcmp(argv[i] + 2, "config-root") == 0 && i + 1 < argc)
				{
					++i;
					configPath = argv[i];
				}
				else if (std::strcmp(argv[i] + 2, "port") == 0 && i + 1 < argc)
				{
					++i;
					gfx.onlinePort = static_cast<uint16_t>(std::atoi(argv[i]));
				}
				break;
			}
		}
		else
		{
			tcName = argv[i];
			tcSet = true;
		}
	}

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD);

	initKeys();

	precomputeTables();

	gfx.loadMenus();

	gfx.init();
	gfx.setConfigPath(configPath);

	FsNode configNode(gfx.getConfigNode());

	if (!gfx.loadSettings(configNode / "Setups" / "liero.cfg"))
	{
		if (!gfx.loadSettingsLegacy(configNode / "LIERO.DAT"))
		{
			gfx.settings.reset(new Settings);
			gfx.saveSettings(configNode / "Setups" / "liero.cfg");
		}
	}

	if (tcSet)
		gfx.settings->tc = tcName;

	// TC loading
	FsNode lieroRoot(configNode / "TC" / gfx.settings->tc);
	std::shared_ptr<Common> common(new Common());
	common->load(std::move(lieroRoot));
	gfx.common = common;
	gfx.playRenderer.loadPalette(*common); // This gets the palette from common

	gfx.setVideoMode();
	sfx.init();

	gfx.mainLoop();

	gfx.settings->save(configNode / "Setups" / "liero.cfg", gfx.rand);

	sfx.deinit();
	SDL_Quit();

	return 0;
}
catch(std::exception&)
{
	SDL_Quit();
	throw;
}
