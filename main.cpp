#ifndef LIERO_MAIN_HPP
#define LIERO_MAIN_HPP

#include <SDL/SDL.h>
#include <SDL/SDL_getenv.h>

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
#include "platform.hpp"

#include <iostream>
#include <ctime>
#include <exception>
#include <gvl/math/ieee.hpp>

#include <gvl/math/cmwc.hpp>

//#include <gvl/support/profile.hpp> // TEMP

//#undef main

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
	//common->loadPowerlevelPalette = true;
	
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

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
	
/*
	char buf[256];
	std::cout << SDL_VideoDriverName(buf, 256) << std::endl;
*/
	
	common->texts.loadFromEXE();
		
	//common.texts.loadFromEXE();
	initKeys();
	//game.rand.seed(Uint32(std::time(0)));
	common->loadConstantsFromEXE();
	loadTablesFromEXE();

	Console::clear();
	Console::writeTextBar(common->texts.copyright1, common->texts.copyrightBarFormat);
	Console::setAttributes(0x07);
	Console::writeLine("");
	
	Console::write(common->S[LoadingAndThinking]);
	common->font.loadFromEXE();
	common->loadPalette();
	gfx.loadPalette(); // This gets the palette from common
	gfx.loadMenus();
	common->loadGfx();
	common->loadMaterials();
	common->loadWeapons();
	common->loadTextures();
	common->loadOthers();
	Console::writeLine(common->S[OK]);
	
	Console::writeLine(common->S[InitSound]);
	sfx.init();
	
	Console::write(common->S[Init_BaseIO]);
	Console::write("0220");
	Console::write(common->S[Init_IRQ]);
	Console::write("7");
	Console::write(common->S[Init_DMA8]);
	Console::write("1");
	Console::write(common->S[Init_DMA16]);
	Console::writeLine("5");
	
	Console::write(common->S[Init_DSPVersion]);
	SDL_version const* mixerVer = Mix_Linked_Version();
	Console::write(toString(mixerVer->major) + "." + toString(mixerVer->minor));
	Console::write(common->S[Init_Colon]);
	Console::write(common->S[Init_16bit]);
	Console::writeLine(common->S[Init_Autoinit]);
	
	Console::writeLine(common->S[Init_XMSSucc]);
	
	Console::write(common->S[Init_FreeXMS]);
#ifdef LIERO_WIN32
	Console::write(toString(Win32::getFreeMemory()));
#else
	
	Console::write("OVER 9000 ");
#endif
	Console::write(common->S[Init_k]);
	
	Console::write(common->S[LoadingSounds]);
	sfx.loadFromSND();
	Console::writeLine(common->S[OK2]);
	
	Console::writeLine("");
	Console::write(common->S[PressAnyKey]);
	Console::waitForAnyKey();
	Console::clear();
	
	gfx.init();
	
	gfx.settingsFile = "LIERO";
	
#if 0 // This is just stupid, no need to emulate it
	if(!fileExists(lieroOPT)) // NOTE: Liero doesn't seem to use the contents of LIERO.OPT for anything useful
	{
		gfx.settings.reset(new Settings);
		gfx.saveSettings();
	}
	else
#endif
	{
	/*
		FILE* f = fopen(lieroOPT.c_str(), "rb");
		std::size_t len = fileLength(f);
		if(len > 255) len = 255;
		char buf[256];
		fread(buf, 1, len, f);
		game.settingsFile.assign(buf, len);
		
		rtrim(game.settingsFile);
		*/
		if(!gfx.loadSettings())
		{
			gfx.settingsFile = "LIERO";
			gfx.settings.reset(new Settings);
			gfx.saveSettings();
		}

		//fclose(f);
	}
	
	//game.initGame();
	gfx.mainLoop();
	
	gfx.settingsFile = "LIERO";
	gfx.settings->save(joinPath(lieroEXERoot, "LIERO.DAT"));
	
	FILE* f = fopen(lieroOPT.c_str(), "wb");
	fwrite(gfx.settingsFile.data(), 1, gfx.settingsFile.size(), f);
	fputc('\r', f);
	fputc('\n', f);
	fclose(f);
	
	closeAllCachedFiles();
	
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

#endif // LIERO_MAIN_HPP

