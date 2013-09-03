#ifndef UUID_DC1D9513CDD34960AB8A648004DA149D
#define UUID_DC1D9513CDD34960AB8A648004DA149D

#include <SDL/SDL.h>
#if !SDL13
#include <SDL/SDL_getenv.h>
#endif

#include "gfx.hpp"
#include "sfx.hpp"
#include "sys.hpp"
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
#include <gvl/support/platform.hpp>

#include <ctime>
#include <exception>
#include <gvl/math/cmwc.hpp>

int gameEntry(int argc, char *argv[])
try
{
	// TODO: Better PRNG seeding
	gfx.rand.seed(Uint32(std::time(0)));
	
	bool exeSet = false;
	gvl::shared_ptr<Common> common(new Common);
	gfx.common = common;
	
	for(int i = 1; i < argc; ++i)
	{
		if(argv[i][0] == '-')
		{
			switch(argv[i][1])
			{
			case 'v':
				// SDL_putenv seems to take char* in linux, STOOPID
				SDL_putenv(const_cast<char*>((std::string("SDL_VIDEODRIVER=") + &argv[i][2]).c_str()));
			break;
			}
		}
		else
		{
			setLieroEXE(argv[i]);
			exeSet = true;
		}
	}
	
	if(!exeSet)
		setLieroEXE("LIERO.EXE");

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK);
	
	common->texts.loadFromEXE();

	initKeys();
	common->loadConstantsFromEXE();
	loadTablesFromEXE();

	common->font.loadFromEXE();
	common->loadPalette();
	gfx.loadPalette(*common); // This gets the palette from common
	gfx.loadMenus();
	common->loadGfx();
	common->loadMaterials();
	common->loadWeapons();
	common->loadTextures();
	common->loadOthers();

	common->loadSfx();

	gfx.init();
	
	if(!gfx.loadSettings(joinPath(lieroEXERoot, "LIERO")))
	{
		gfx.settings.reset(new Settings);
		gfx.saveSettings(joinPath(lieroEXERoot, "LIERO"));
	}
	
	gfx.setVideoMode();
	sfx.init();
	
	gfx.mainLoop();
	
	gfx.settings->save(joinPath(lieroEXERoot, "LIERO.DAT"), gfx.rand);
	
	sfx.deinit();
	SDL_Quit();
	
	//gvl::present_profile(std::cout);
	
	return 0;
}
catch(std::exception&)
{
	SDL_Quit();
	throw;
}

#endif // UUID_DC1D9513CDD34960AB8A648004DA149D
