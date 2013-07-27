#ifndef UUID_8615289728154E2FB9B179C2745D5FA9
#define UUID_8615289728154E2FB9B179C2745D5FA9

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

#include <iostream>
#include <ctime>
#include <exception>
#include <gvl/math/ieee.hpp>

#include <gvl/math/cmwc.hpp>

//#include <gvl/support/profile.hpp> // TEMP
//#include <gvl/support/log.hpp> // TEMP

int gameEntry(int argc, char* argv[])
try
{
	gvl_init_ieee();

	// TODO: Better PRNG seeding
	Console::init();
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
			/*
			case 'r':
				common->loadPowerlevelPalette = false;
			break;*/
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

	Console::clear();

	common->font.loadFromEXE();
	common->loadPalette();
	gfx.loadPalette(); // This gets the palette from common
	gfx.loadMenus();
	common->loadGfx();
	common->loadMaterials();
	common->loadWeapons();
	common->loadTextures();
	common->loadOthers();
	sfx.loadFromSND();
	gfx.init();
	
	gfx.settingsFile = "LIERO";

	SDL_Quit();
	
	return 0;
}
catch(std::exception& ex)
{
	SDL_Quit();
	Console::setAttributes(0x2f);
	Console::writeLine(std::string("EXCEPTION: ") + ex.what());
	Console::writeLine("Press any key to quit");
	Console::waitForAnyKey();
	return 1;
}

#endif // UUID_8615289728154E2FB9B179C2745D5FA9
