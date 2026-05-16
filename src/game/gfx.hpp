#pragma once

#include <SDL3/SDL.h>
#include <gvl/math/rect.hpp>

#include <cstdio>
#include <cassert>

#include "state.hpp"
#include "gfx/font.hpp"
#include "gfx/blit.hpp"
#include "gfx/color.hpp"
#include "gfx/bitmap.hpp"
#include "gfx/renderer.hpp"
#include "menu/menu.hpp"
#include "menu/hiddenMenu.hpp"
#include "menu/mainMenu.hpp"
#include "rand.hpp"
#include "keys.hpp"
#include "settings.hpp"
#include "common.hpp"
#include "filesystem.hpp"

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

	enum
	{
		PlName,
		PlHealth,
		PlRed, PlGreen, PlBlue,
		PlUp,
		PlDown,
		PlLeft,
		PlRight,
		PlFire,
		PlChange,
		PlJump,
		PlDig,
		PlWeap0,
		PlController = PlWeap0 + 5,
		PlSaveProfile,
		PlSaveProfileAs,
		PlLoadProfile,
		PlLoadedProfile,
	};

	virtual void drawItemOverlay(Common& common, MenuItem& item, int x, int y, bool selected, bool disabled);

	virtual ItemBehavior* getItemBehavior(Common& common, MenuItem& item);

	std::shared_ptr<WormSettings> ws;
};

struct SettingsMenu : Menu
{
	enum
	{
		SiGameMode,
		SiLives,
		SiTimeToLose, // Extra
		SiTimeToWin,
		SiZoneTimeout,
		SiFlagsToWin, // Extra
		SiLoadingTimes,
		SiMaxBonuses,
		SiNamesOnBonuses,
		SiMap,
		SiAmountOfBlood,
		SiLevel,
		SiRegenerateLevel,
		SiWeaponOptions,
		LoadOptions,
		SaveOptions,
		LoadChange,
	};

	SettingsMenu(int x, int y)
	: Menu(x, y)
	{
	}

	virtual ItemBehavior* getItemBehavior(Common& common, MenuItem& item);

	virtual void onUpdate();
};

struct Joystick {
	SDL_Gamepad *sdlGamepad;
	bool btnState[MaxJoyButtons];

	void clearState() {
		for ( int i = 0; i < MaxJoyButtons; ++i ) btnState[i] = false;
	}
};


struct Gfx
{
	Gfx();

	void init();
	void setVideoMode();
	void onWindowResize(uint32_t windowId);
	void loadMenus();

	void process(Controller* controller = 0);
	// draws a given surface onto an SDL texture/renderer, using a given Renderer
	void draw(SDL_Surface& surface, SDL_Texture& texture, SDL_Renderer& sdlRenderer, Renderer& renderer);
	void flip();
	void menuFlip(bool quitting = false);

	void clearKeys();

	bool testKeyOnce(uint32_t key)
	{
		bool state = dosKeys[key];
		dosKeys[key] = false;
		return state;
	}

	bool testKey(uint32_t key)
	{
		return dosKeys[key];
	}

	void releaseKey(uint32_t key)
	{
		dosKeys[key] = false;
	}

	void pressKey(uint32_t key)
	{
		dosKeys[key] = true;
	}

	void setKey(uint32_t key, bool state)
	{
		dosKeys[key] = state;
	}

	void toggleKey(uint32_t key)
	{
		dosKeys[key] = !dosKeys[key];
	}

	bool testSDLKeyOnce(SDL_Scancode key)
	{
		uint32_t k = SDLToDOSKey(key);
		return k ? testKeyOnce(k) : false;
	}

	bool testSDLKey(SDL_Scancode key)
	{
		uint32_t k = SDLToDOSKey(key);
		return k ? testKey(k) : false;
	}

	void releaseSDLKey(SDL_Scancode key)
	{
		uint32_t k = SDLToDOSKey(key);
		if(k)
			dosKeys[k] = false;
	}

	std::string getKeyName(uint32_t key);
	void setSpectatorFullscreen(bool newFullscreen);
	void setFullscreen(bool newFullscreen);
	void setDoubleRes(bool newDoubleRes);

	void saveSettings(FsNode node);
	bool loadSettings(FsNode node);
	bool loadSettingsLegacy(FsNode node);

	void processEvent(SDL_Event& ev, Controller* controller = 0);

	int menuLoop();
	void mainLoop();

	// Initialize the state stack for frame-stepped operation.
	// Call once before calling runOneFrame() in a loop.
	void initFrameStepping();

	// Advance the application by one frame. Returns false when the
	// application should exit (quit selected or TC change requested).
	// After a TC change, call initFrameStepping() again.
	bool runOneFrame();

	// True if a TC change was requested (caller should reload and re-init)
	bool tcChangeRequested() const { return tcChangeRequested_; }

	void drawBasicMenu(/*int curSel*/);
	void drawSpectatorInfo();
	void playerSettings(int player);
	void openHiddenMenu();

	static void preparePalette(SDL_PixelFormatDetails const* format, SDL_Palette const* palette, Color realPal[256], uint32_t (&pal32)[256]);

	static void overlay(
		SDL_PixelFormatDetails const* format,
		uint8_t* src, int w, int h, std::size_t srcPitch,
		uint8_t* dest, std::size_t destPitch, int mag);

	void setConfigPath(std::string const& path)
	{
		configNode = FsNode(path);
	}

	FsNode getConfigNode()
	{
		return configNode;
	}

	// PRNG for things that don't affect the game
	Rand rand;

	// renders everything for actual play
	Renderer playRenderer;
	// renders everything on a single screen, for single screen replay and
	// the spectator window
	Renderer singleScreenRenderer;

	// the renderer currently in use by the primary window. Usually
	// playRenderer, but is singleScreenRenderer if watching a replay in
	// single screen mode.
	Renderer* primaryRenderer;

	FsNode configNode;

	MainMenu mainMenu;
	SettingsMenu settingsMenu;
	PlayerMenu playerMenu;
	HiddenMenu hiddenMenu;

	Menu* curMenu;
	std::string prevSelectedReplayPath;
	FsNode settingsNode; // Currently loaded settings file. TODO: This is only used for display. We could just remember the name.
	std::shared_ptr<Settings> settings;

	bool dosKeys[177];
	// the window to render into
	SDL_Window* sdlWindow = NULL;
	// the window to render the spectator view into
	SDL_Window* sdlSpectatorWindow = NULL;
	// the SDL renderer to use
	SDL_Renderer* sdlRenderer = NULL;
	// the SDL renderer to use for the spectator window
	SDL_Renderer* sdlSpectatorRenderer = NULL;
	// full window size texture that represents the window
	SDL_Texture* sdlTexture = NULL;
	// full spectator window size texture that represents the spectator window
	SDL_Texture* sdlSpectatorTexture = NULL;
	// a software surface to do the actual drawing into
	SDL_Surface* sdlDrawSurface = NULL;
	// a software surface to do the actual drawing of the spectator view into
	SDL_Surface* sdlSpectatorDrawSurface = NULL;
	// when the menu is open, the ongoing game on the screen is paused and
	// stored in this bitmap
	Bitmap frozenScreen;
	// when the menu is open, the ongoing game on the spectator screen is
	// paused and stored in this bitmap
	Bitmap frozenSpectatorScreen;

	bool running;
	bool spectatorFullscreen, doubleRes;

	uint64_t lastFrame;
	unsigned menuCycles;
	int windowW, windowH;
	int prevMag; // Previous magnification used for drawing
	gvl::rect lastUpdateRect; // Last region that was updated when flipping
	std::shared_ptr<Common> common;
	std::unique_ptr<Controller> controller;

	StateStack stateStack;

	// Used by sub-states to communicate a menu selection back to MainMenuState
	int pendingMenuSelection = -1;

	// Error message to display after GamePlayState pops (set by controllers)
	std::string pendingErrorMessage;

	std::vector<Joystick> joysticks;

	SDL_Scancode keyBuf[32], *keyBufPtr;

	std::vector<std::pair<int, int>> debugPoints;
	std::string debugInfo;

private:
	struct MainMenuState* menuStatePtr_ = nullptr;
	bool tcChangeRequested_ = false;
};

extern Gfx gfx;
