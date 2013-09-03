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

//#include <iostream>
#include <ctime>
#include <exception>
#include <gvl/math/cmwc.hpp>

//#include <gvl/support/profile.hpp> // TEMP
//#include <gvl/support/log.hpp> // TEMP

#if __APPLE__
#define gameEntry SDL_main
#endif

int gameEntry(int argc, char* argv[])
try
{
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
	
/*
	char buf[256];
	std::cout << SDL_VideoDriverName(buf, 256) << std::endl;
*/
	
	common->texts.loadFromEXE();

	initKeys();
	common->loadConstantsFromEXE();
	loadTablesFromEXE();

	//Console::clear();
	//Console::writeTextBar(common->texts.copyright1, common->texts.copyrightBarFormat);
	//Console::setAttributes(0x07);
	//Console::writeLine("");
	
	//Console::write(common->S[LoadingAndThinking]);
	common->font.loadFromEXE();
	common->loadPalette();
	gfx.loadPalette(*common); // This gets the palette from common
	gfx.loadMenus();
	common->loadGfx();
	common->loadMaterials();
	common->loadWeapons();
	common->loadTextures();
	common->loadOthers();
	//Console::writeLine(common->S[OK]);
	
	//Console::writeLine(common->S[InitSound]);
	
	//Console::write(common->S[Init_BaseIO]);
	//Console::write("0220");
	//Console::write(common->S[Init_IRQ]);
	//Console::write("7");
	//Console::write(common->S[Init_DMA8]);
	//Console::write("1");
	//Console::write(common->S[Init_DMA16]);
	//Console::writeLine("5");
	
#if !DISABLE_SOUND	
	//Console::write(common->S[Init_DSPVersion]);
	//Console::write(toString(0) + "." + toString(1));
	//Console::write(common->S[Init_Colon]);
	//Console::write(common->S[Init_16bit]);
	//Console::writeLine(common->S[Init_Autoinit]);
#endif	
	//Console::writeLine(common->S[Init_XMSSucc]);
	
	//Console::write(common->S[Init_FreeXMS]);
#if GVL_WIN32
	//Console::write(toString(Win32::getFreeMemory()));
#else
	
	//Console::write("OVER 9000 ");
#endif
	//Console::write(common->S[Init_k]);
	
	//Console::write(common->S[LoadingSounds]);
	common->loadSfx();
	//Console::writeLine(common->S[OK2]);
	
	//Console::writeLine("");
	//Console::write(common->S[PressAnyKey]);
	//Console::waitForAnyKey();
	//Console::clear();
	
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
catch(std::exception& ex)
{
	SDL_Quit();
	Console::setAttributes(0x2f);
	Console::writeLine(std::string("EXCEPTION: ") + ex.what());
	Console::writeLine("Press any key to quit");
	Console::waitForAnyKey();
	return 1;
}

#endif // UUID_DC1D9513CDD34960AB8A648004DA149D
