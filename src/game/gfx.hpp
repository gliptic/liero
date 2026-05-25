#pragma once

#include <SDL3/SDL.h>
#include "math/rect.hpp"

#include <cstdio>
#include <cassert>
#include <unordered_map>

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
struct NetSession;

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
		PlInput,
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
	SDL_JoystickID instanceId;
	bool btnState[SDL_GAMEPAD_BUTTON_COUNT];
	bool btnPressed[SDL_GAMEPAD_BUTTON_COUNT]; // Latched on press, cleared by testGamepadButtonOnce
	bool axisButtonState[12]; // 6 axes * 2 directions
	bool axisPressed[12];     // Latched on axis threshold cross, cleared by consumer

	void clearState() {
		std::memset(btnState, 0, sizeof(btnState));
		std::memset(btnPressed, 0, sizeof(btnPressed));
		std::memset(axisButtonState, 0, sizeof(axisButtonState));
		std::memset(axisPressed, 0, sizeof(axisPressed));
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

	// Test any key (both regular DOS keys and extended joystick keys)
	bool testAnyKeyOnce(uint32_t key)
	{
		if(key == 0) return false;
		if(key < MaxDOSKey)
			return testKeyOnce(key);
		auto it = exKeys.find(key);
		if(it != exKeys.end() && it->second)
		{
			it->second = false;
			return true;
		}
		return false;
	}

	bool testAnyKey(uint32_t key)
	{
		if(key == 0) return false;
		if(key < MaxDOSKey)
			return testKey(key);
		auto it = exKeys.find(key);
		return it != exKeys.end() && it->second;
	}

	void releaseAnyKey(uint32_t key)
	{
		if(key == 0) return;
		if(key < MaxDOSKey)
			dosKeys[key] = false;
		else
			exKeys[key] = false;
	}

	// Test if any player's configured control for a given action was pressed.
	// Uses controlsEx which covers both keyboard and joystick bindings.
	bool testControlOnce(int control);

	// Test if any connected gamepad has a raw button pressed (one-shot)
	bool testGamepadButtonOnce(int button);

	// Test if any connected gamepad has a raw button held (non-destructive)
	bool testGamepadButton(int button);

	// Directional input: checks both DPad button AND left stick axis (one-shot)
	bool testGamepadDirOnce(int dpadButton);

	// Directional input: checks both DPad button AND left stick axis (held)
	bool testGamepadDir(int dpadButton);

	// Non-destructive version for held keys (left/right repeat)
	bool testControl(int control);

	// Release a control key for all players
	void releaseControl(int control);

	std::string getKeyName(uint32_t key);
	std::string getGamepadKeyName(uint32_t gamepadKey);
	void setSpectatorFullscreen(bool newFullscreen);
	void setFullscreen(bool newFullscreen);
	void setDoubleRes(bool newDoubleRes);

	void saveSettings(FsNode node);
	bool loadSettings(FsNode node);

	void processEvent(SDL_Event& ev, Controller* controller = 0);

	int findGamepadIndex(SDL_JoystickID id);
	int findGamepadForPlayer(int playerIdx);
	void dispatchGamepadInput(int gpIdx, uint32_t gamepadKey, bool state, Controller* controller);

	void mainLoop();

	// Initialize the state stack.
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

	// Port for online play (default 19532, configurable via --port)
	uint16_t onlinePort = 19532;

	MainMenu mainMenu;
	SettingsMenu settingsMenu;
	PlayerMenu playerMenu;
	HiddenMenu hiddenMenu;

	Menu* curMenu;
	std::string prevSelectedReplayPath;
	FsNode settingsNode; // Currently loaded settings file. TODO: This is only used for display. We could just remember the name.
	std::shared_ptr<Settings> settings;

	bool dosKeys[177];
	std::unordered_map<uint32_t, bool> exKeys;

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
	Rect lastUpdateRect; // Last region that was updated when flipping
	std::shared_ptr<Common> common;
	std::unique_ptr<Controller> controller;
	std::unique_ptr<NetSession> netSession;
	std::string pendingNetAddress;

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
