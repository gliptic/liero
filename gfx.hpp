#ifndef LIERO_GFX_HPP
#define LIERO_GFX_HPP

#include <SDL/SDL.h>
#include <gvl/resman/shared_ptr.hpp>
#include <gvl/math/rect.hpp>

#include <cstdio>
#include <cassert>

#include "gfx/font.hpp"
#include "gfx/blit.hpp"
#include "gfx/color.hpp"
#include "gfx/bitmap.hpp"
#include "gfx/renderer.hpp"
#include "menu/menu.hpp"
#include "menu/hiddenMenu.hpp"
#include "rect.hpp"
#include "rand.hpp"
#include "keys.hpp"
#include "settings.hpp"
#include "common.hpp"


struct Key
{
	Key(int sym, char ch)
	: sym(sym), ch(ch)
	{
	}
	
	int sym;
	char ch;
};



struct Game;
struct Controller;
struct Gfx;

struct PlayerMenu : Menu
{
	PlayerMenu(int x, int y)
	: Menu(x, y)
	{
	}
	
	virtual void drawItemOverlay(Common& common, int item, int x, int y, bool selected, bool disabled);
	
	virtual ItemBehavior* getItemBehavior(Common& common, int item);
	
	gvl::shared_ptr<WormSettings> ws;
};

struct SettingsMenu : Menu
{
	SettingsMenu(int x, int y)
	: Menu(x, y)
	{
	}
	
	virtual ItemBehavior* getItemBehavior(Common& common, int item);
};

enum
{
	MaResumeGame = 0,
	MaNewGame = 1,
	MaSettings = 2,
	MaPlayer1Settings = 3,
	MaPlayer2Settings = 4,
	MaAdvanced = 5,
	MaQuit = 6,
	MaReplay = 7
};

struct Joystick {
	SDL_Joystick *sdlJoystick;
	bool btnState[MaxJoyButtons];
	
	void clearState() {
		for ( int i = 0; i < MaxJoyButtons; ++i ) btnState[i] = false;
	}
};


struct Gfx : Renderer
{
	Gfx();
		
	void init();
	void setVideoMode();
	void loadMenus();
	
	void process(Controller* controller = 0);
	void flip();
	void menuFlip();
	
	void clearKeys();
	
	bool testKeyOnce(Uint32 key)
	{
		bool state = dosKeys[key];
		dosKeys[key] = false;
		return state;
	}
	
	bool testKey(Uint32 key)
	{
		return dosKeys[key];
	}
	
	void releaseKey(Uint32 key)
	{
		dosKeys[key] = false;
	}
	
	void pressKey(Uint32 key)
	{
		dosKeys[key] = true;
	}
	
	void setKey(Uint32 key, bool state)
	{
		dosKeys[key] = state;
	}
	
	void toggleKey(Uint32 key)
	{
		dosKeys[key] = !dosKeys[key];
	}
	
	bool testSDLKeyOnce(SDLKey key)
	{
		Uint32 k = SDLToDOSKey(key);
		return k ? testKeyOnce(k) : false;
	}
	
	bool testSDLKey(SDLKey key)
	{
		Uint32 k = SDLToDOSKey(key);
		return k ? testKey(k) : false;
	}
	
	void releaseSDLKey(SDLKey key)
	{
		Uint32 k = SDLToDOSKey(key);
		if(k)
			dosKeys[k] = false;
	}
	
	SDL_keysym waitForKey();
	uint32_t waitForKeyEx();
	std::string getKeyName(uint32_t key);
	void setFullscreen(bool newFullscreen);
	void setDoubleRes(bool newDoubleRes);
	
	void saveSettings(std::string const& path);
	bool loadSettings(std::string const& path);
	
	void processEvent(SDL_Event& ev, Controller* controller = 0);
	
	void updateSettingsMenu();
	int menuLoop();
	void mainLoop();
	void drawBasicMenu(/*int curSel*/);
	void playerSettings(int player);
	void openHiddenMenu();

	bool inputString(std::string& dest, std::size_t maxLen, int x, int y, int (*filter)(int) = 0, std::string const& prefix = "", bool centered = true);
	void inputInteger(int& dest, int min, int max, std::size_t maxLen, int x, int y);
	void selectLevel();
	int  selectReplay();
	void selectProfile(WormSettings& ws);
	void selectOptions();
	void weaponOptions();
	void infoBox(std::string const& text, int x = 320/2, int y = 200/2, bool clearScreen = true);

	static void preparePalette(SDL_PixelFormat* format, Color realPal[256], uint32_t (&pal32)[256]);
	
	static void overlay(
		SDL_PixelFormat* format,
		uint8_t* src, int w, int h, std::size_t srcPitch,
		uint8_t* dest, std::size_t destPitch, int mag);

	Menu mainMenu;
	SettingsMenu settingsMenu;
	PlayerMenu playerMenu;
	HiddenMenu hiddenMenu;
	
	Menu* curMenu;
	std::string settingsFile; // Currently loaded settings file
	gvl::shared_ptr<Settings> settings;
	
	bool dosKeys[177];
	SDL_Surface* back;
	std::vector<PalIdx> frozenScreen;

	bool running;
	bool fullscreen, doubleRes;
	
	Uint32 lastFrame;
	unsigned menuCycles;
	int windowW, windowH;
	int prevMag; // Previous magnification used for drawing
	gvl::rect lastUpdateRect; // Last region that was updated when flipping
	gvl::shared_ptr<Common> common;
	std::auto_ptr<Controller> controller;
	
	std::vector<Joystick> joysticks;

	SDL_keysym keyBuf[32], *keyBufPtr;
	
	std::vector<std::pair<int, int>> debugPoints;
	std::string debugInfo;
};

extern Gfx gfx;

#endif // LIERO_GFX_HPP
