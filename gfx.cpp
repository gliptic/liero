#include "gfx.hpp"
#include "reader.hpp"
#include "game.hpp"
#include "sfx.hpp"
#include "text.hpp"
#include "keys.hpp"
#include "filesystem.hpp"
#include <cstring>
#include <cassert>
#include <cstdlib>
#include <cctype>
#include <vector>
#include <utility>
#include <algorithm>
#include <SDL/SDL.h>
#include <cstdio>
#include <memory>

#include "controller/replayController.hpp"
#include "controller/localController.hpp"
#include "controller/controller.hpp"

#include "gfx/macros.hpp"

#include "menu/arrayEnumBehavior.hpp"
#include "menu/fileSelector.hpp"

Gfx gfx;

struct KeyBehavior : ItemBehavior
{
	KeyBehavior(Common& common, uint32_t& key, uint32_t& keyEx, bool extended = false)
	: common(common)
	, key(key)
	, keyEx(keyEx)
	, extended(extended)
	{
	}
	
	int onEnter(Menu& menu, int item)
	{
		sfx.play(common, 27);
		uint32_t k;
		bool isEx;
		do
		{
			k = gfx.waitForKeyEx();
			isEx = isExtendedKey(k);
		}
		while(!extended && isEx);

		if(k != DkEscape)
		{
			if(!isEx)
				key = k;
			keyEx = k;
			
			onUpdate(menu, item);
		}
		
		gfx.clearKeys();
		return -1;
	}
	
	void onUpdate(Menu& menu, int item)
	{
		menu.items[item].value = gfx.getKeyName(extended ? keyEx : key);
	}
	
	Common& common;
	uint32_t& key;
	uint32_t& keyEx;
	bool extended;
};

struct WormNameBehavior : ItemBehavior
{
	WormNameBehavior(Common& common, WormSettings& ws)
	: common(common)
	, ws(ws)
	{
	}
	
	int onEnter(Menu& menu, int item)
	{
		sfx.play(common, 27);
		
		ws.randomName = false;
		gfx.inputString(ws.name, 20, 275, 20);
		
		if(ws.name.empty())
		{
			Settings::generateName(ws, gfx.rand);
		}
		sfx.play(common, 27);
		onUpdate(menu, item);
		return -1;
	}
	
	void onUpdate(Menu& menu, int item)
	{
		menu.items[item].value = ws.name;
	}
	
	Common& common;
	WormSettings& ws;
};


struct ProfileSaveBehavior : ItemBehavior
{
	ProfileSaveBehavior(Common& common, WormSettings& ws, bool saveAs = false)
	: common(common)
	, ws(ws)
	, saveAs(saveAs)
	{
	}
	
	int onEnter(Menu& menu, int item)
	{
		sfx.play(common, 27);
		
		int x, y;
		if(!menu.itemPosition(item, x, y))
			return -1;
			
		x += menu.valueOffsetX + 2;
		
		if(saveAs)
		{
			std::string name;
			if(gfx.inputString(name, 30, x, y) && !name.empty())
			{
				ws.saveProfile(joinPath(joinPath(lieroEXERoot, "Profiles"), name));
			}
				
			sfx.play(common, 27);
		}
		else
			ws.saveProfile(ws.profilePath);
		
		menu.updateItems(common);
		return -1;
	}
	
	void onUpdate(Menu& menu, int item)
	{
		if(saveAs)
		{
			menu.items[item].value = getLeaf(ws.profilePath);
			menu.items[item].hasValue = true;
		}
		else
		{
			menu.items[item].visible = !ws.profilePath.empty();
		}
	}
	
	Common& common;
	WormSettings& ws;
	bool saveAs;
};


Gfx::Gfx()
: mainMenu(53, 20)
, settingsMenu(178, 20)
, playerMenu(178, 20)
, hiddenMenu(178, 20)
, curMenu(0)
, back(0)
, frozenScreen(320 * 200)
, running(true)
, fullscreen(false)
, menuCycles(0)
, windowW(320 * 2)
, windowH(200 * 2)
, prevMag(0)
, keyBufPtr(keyBuf)
, doubleRes(true)
{
	clearKeys();
}

void Gfx::init()
{
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
	SDL_EnableUNICODE(1);
	SDL_WM_SetCaption("Liero 1.36", 0);
	SDL_ShowCursor(SDL_DISABLE);
	lastFrame = SDL_GetTicks();

	Renderer::init();
	
	// Joystick init:
	SDL_JoystickEventState(SDL_ENABLE);
	int numJoysticks = SDL_NumJoysticks();
	joysticks.resize(numJoysticks);
	for ( int i = 0; i < numJoysticks; ++i ) {
		joysticks[i].sdlJoystick = SDL_JoystickOpen(i);
		joysticks[i].clearState();
	}
}

void Gfx::setVideoMode()
{
	int bitDepth = 32;
	
	int flags = SDL_SWSURFACE | SDL_RESIZABLE;
	if(fullscreen)
	{
		flags |= SDL_FULLSCREEN;
		if(settings->fullscreenW > 0 && settings->fullscreenH > 0)
		{
			windowW = settings->fullscreenW;
			windowH = settings->fullscreenH;
		}
	}
	
	if(!SDL_VideoModeOK(windowW, windowH, bitDepth, flags))
	{
		// Default to 640x480
		windowW = 640;
		windowH = 480;
	}

	back = SDL_SetVideoMode(windowW, windowH, bitDepth, flags);

	doubleRes = (windowW >= 640 && windowH >= 400);
}

void Gfx::loadMenus()
{
	ReaderFile& exe = openLieroEXE();

	hiddenMenu.addItem(MenuItem(48, 7, "RECORD REPLAYS"));
	hiddenMenu.addItem(MenuItem(48, 7, "LOAD REPLAY..."));
	hiddenMenu.addItem(MenuItem(48, 7, "POWERLEVEL PALETTES"));
	hiddenMenu.addItem(MenuItem(48, 7, "SCALING FILTER"));
	hiddenMenu.addItem(MenuItem(48, 7, "DOUBLE RES (F6)"));
	hiddenMenu.addItem(MenuItem(48, 7, "FULLSCREEN (F5)"));
	hiddenMenu.addItem(MenuItem(48, 7, "FULLSCREEN WIDTH"));
	hiddenMenu.addItem(MenuItem(48, 7, "FULLSCREEN HEIGHT"));
	hiddenMenu.addItem(MenuItem(48, 7, "AI FRAMES"));
	hiddenMenu.addItem(MenuItem(48, 7, "AI MUTATIONS"));
	hiddenMenu.addItem(MenuItem(48, 7, "PALETTE"));
	hiddenMenu.addItem(MenuItem(48, 7, "LOAD OPTIONS..."));
	hiddenMenu.addItem(MenuItem(48, 7, "SAVE OPTIONS..."));
	
	exe.seekg(0x1B08A);
	mainMenu.readItems(exe, 14, 3, true);
	int p = exe.tellg();

	exe.seekg(0x1B0C2);
	settingsMenu.readItems(exe, 21, 9, false, 48, 7);
	hiddenMenu.readItems(exe, 21, 3, false, 48, 7); // Shadows, ScreenSync, LoadChange
	mainMenu.readItems(exe, 21, 2, false, 48, 48); // Player settings
	settingsMenu.readItems(exe, 21, 1, false, 48, 7);

	mainMenu.addItem(MenuItem(48, 48, "ADVANCED"));

	exe.seekg(p);
	mainMenu.readItems(exe, 14, 1, true); // QUIT TO OS
	
	settingsMenu.valueOffsetX = 100;
	
	settingsMenu.items[Settings::SiLives].string = common->texts.gameModeSpec[0];
	settingsMenu.addItem(MenuItem(48, 7, common->texts.gameModeSpec[1]), Settings::SiTimeToLose);
	settingsMenu.addItem(MenuItem(48, 7, "TIME TO WIN"), Settings::SiTimeToWin);
	settingsMenu.addItem(MenuItem(48, 7, "ZONE TIMEOUT"), Settings::SiZoneTimeout);
	settingsMenu.addItem(MenuItem(48, 7, common->texts.gameModeSpec[2]), Settings::SiFlagsToWin);
	
	for(int i = 0; i < Settings::SiWeaponOptions; ++i)
	{
		settingsMenu.items[i].hasValue = true;
	}
	
	exe.seekg(0x1B210);
	playerMenu.readItems(exe, 13, 12, false, 48, 7);
	
	// Extra control settings:
	playerMenu.addItem(MenuItem(48, 7, "DIG"));
	
	// Finish reading liero menus:
	playerMenu.readItems(exe, 13, 1, false, 48, 7);
	playerMenu.valueOffsetX = 95;
	
	playerMenu.addItem(MenuItem(3, 7, "SAVE PROFILE"));
	playerMenu.addItem(MenuItem(3, 7, "SAVE PROFILE AS..."));
	playerMenu.addItem(MenuItem(3, 7, "LOAD PROFILE"));
	
	for(int i = 0; i < 14; ++i)
	{
		playerMenu.items[i].hasValue = true;
	}
	
	hiddenMenu.addItem(MenuItem(48, 7, "BOT WEAPONS"));
	
	hiddenMenu.valueOffsetX = 120;
}

void Gfx::updateSettingsMenu()
{
	settingsMenu.items[Settings::SiGameMode].value = common->texts.gameModes[settings->gameMode];
	
	settingsMenu.setVisibility(Settings::SiLives, false);
	settingsMenu.setVisibility(Settings::SiTimeToLose, false);
	settingsMenu.setVisibility(Settings::SiTimeToWin, false);
	settingsMenu.setVisibility(Settings::SiZoneTimeout, false);
	settingsMenu.setVisibility(Settings::SiFlagsToWin, false);
	
	switch(settings->gameMode)
	{
		case Settings::GMKillEmAll:
			settingsMenu.setVisibility(Settings::SiLives, true);
		break;
		
		case Settings::GMGameOfTag:
			settingsMenu.setVisibility(Settings::SiTimeToLose, true);
		break;

		case Settings::GMHoldazone:
			settingsMenu.setVisibility(Settings::SiTimeToWin, true);
			settingsMenu.setVisibility(Settings::SiZoneTimeout, true);
		break;
	}
}

void Gfx::setFullscreen(bool newFullscreen)
{
	if (newFullscreen == fullscreen)
		return;
	fullscreen = newFullscreen;
	if(fullscreen)
	{
		// Try lowest resolution
		windowW = 320;
		windowH = 200;
	}
	setVideoMode();
	hiddenMenu.updateItems(*common);
}

void Gfx::setDoubleRes(bool newDoubleRes)
{
	if (newDoubleRes == doubleRes)
		return;

	if (!newDoubleRes)
	{
		windowW = 320;
		windowH = 200;
	}
	else
	{
		windowW = 640;
		windowH = 400;
	}
	setVideoMode();
	hiddenMenu.updateItems(*common);
}

void Gfx::processEvent(SDL_Event& ev, Controller* controller)
{
	switch(ev.type)
	{
		case SDL_KEYDOWN:
		{
		
			SDLKey s = ev.key.keysym.sym;

			if (keyBufPtr < keyBuf + 32)
				*keyBufPtr++ = ev.key.keysym;

			Uint32 dosScan = SDLToDOSKey(ev.key.keysym);
			if(dosScan)
			{
				dosKeys[dosScan] = true;
				if(controller)
					controller->onKey(dosScan, true);
			}
				
#if 0
			std::cout << "v " << s << ", " << std::hex << ev.key.keysym.mod << ", " << std::dec << int(ev.key.keysym.scancode) << std::endl;
#endif
			
			if(((ev.key.keysym.mod & KMOD_ALT) && s == SDLK_RETURN)
			|| s == SDLK_F5)
			{
				setFullscreen(!fullscreen);
			}
			else if(s == SDLK_F6)
			{
				setDoubleRes(!doubleRes);
			}
		}
		break;
		
		case SDL_KEYUP:
		{
			SDLKey s = ev.key.keysym.sym;
			
			Uint32 dosScan = SDLToDOSKey(s);
			if(dosScan)
			{
				dosKeys[dosScan] = false;
				if(controller)
					controller->onKey(dosScan, false);
			}
				
#if 0
			std::cout << "^ " << s << ", " << std::hex << ev.key.keysym.mod << ", " << std::dec << int(ev.key.keysym.scancode) << std::endl;
#endif
		}
		break;
		
		case SDL_VIDEORESIZE:
		{
			windowW = ev.resize.w;
			windowH = ev.resize.h;
			setVideoMode();
		}
		break;
		
		case SDL_QUIT:
		{
			running = false;
		}
		break;
		
		case SDL_JOYAXISMOTION:
		{
			Joystick& js = joysticks[ev.jbutton.which];
			int jbtnBase = 4 + 2 * ev.jaxis.axis;
			
			bool newBtnStates[2];
			newBtnStates[0] = (ev.jaxis.value > JoyAxisThreshold);
			newBtnStates[1] = (ev.jaxis.value < -JoyAxisThreshold);
			
			for(int i = 0; i < 2; ++i)
			{
				int jbtn = jbtnBase + i;
				bool newState = newBtnStates[i];
				
				if(newState != js.btnState[jbtn])
				{
					js.btnState[jbtn] = newState;
					if (controller)
						controller->onKey(joyButtonToExKey(ev.jbutton.which, jbtn), newState);
				}
			}
		}
		break;
		
		case SDL_JOYHATMOTION:
		{
			Joystick& js = joysticks[ev.jbutton.which];
			
			bool newBtnStates[4];
			newBtnStates[0] = (ev.jhat.value & SDL_HAT_UP) != 0;
			newBtnStates[1] = (ev.jhat.value & SDL_HAT_DOWN) != 0;
			newBtnStates[2] = (ev.jhat.value & SDL_HAT_LEFT) != 0;
			newBtnStates[3] = (ev.jhat.value & SDL_HAT_RIGHT) != 0;
			
			for(int jbtn = 0; jbtn < 4; ++jbtn)
			{
				bool newState = newBtnStates[jbtn];
				if(newState != js.btnState[jbtn])
				{
					js.btnState[jbtn] = newState;
					if(controller)
						controller->onKey(joyButtonToExKey(ev.jbutton.which, jbtn), newState);
				}
			}
		}
		break;
		
		case SDL_JOYBUTTONDOWN:
		case SDL_JOYBUTTONUP: /* Fall-through */
		{
			Joystick& js = joysticks[ev.jbutton.which];
			int jbtn = 16 + ev.jbutton.button;
			js.btnState[jbtn] = (ev.jbutton.state == SDL_PRESSED);
			if(controller)
				controller->onKey(joyButtonToExKey(ev.jbutton.which, jbtn), js.btnState[jbtn]);
		}
		break;
	}
}

void Gfx::process(Controller* controller)
{
	SDL_Event ev;
	keyBufPtr = keyBuf;
	while(SDL_PollEvent(&ev))
	{
		processEvent(ev, controller);
	}
}

SDL_keysym Gfx::waitForKey()
{
	SDL_Event ev;
	while(SDL_WaitEvent(&ev))
	{
		processEvent(ev);
		if(ev.type == SDL_KEYDOWN)
		{
			return ev.key.keysym;
		}
	}
	
	return SDL_keysym(); // Dummy
}

uint32_t Gfx::waitForKeyEx()
{
	SDL_Event ev;
	while(SDL_WaitEvent(&ev))
	{
		processEvent(ev);
		switch (ev.type)
		{
		case SDL_KEYDOWN:
			return SDLToDOSKey(ev.key.keysym);
			
		case SDL_JOYAXISMOTION:
			if(ev.jaxis.value > JoyAxisThreshold)
				return joyButtonToExKey( ev.jbutton.which, 4 + 2 * ev.jaxis.axis );
			else if ( ev.jaxis.value < -JoyAxisThreshold )
				return joyButtonToExKey( ev.jbutton.which, 5 + 2 * ev.jaxis.axis );

			break;
		case SDL_JOYHATMOTION:
			if(ev.jhat.value & SDL_HAT_UP)
				return joyButtonToExKey(ev.jbutton.which, 0);
			else if(ev.jhat.value & SDL_HAT_DOWN)
				return joyButtonToExKey(ev.jbutton.which, 1);
			else if (ev.jhat.value & SDL_HAT_LEFT)
				return joyButtonToExKey(ev.jbutton.which, 2);
			else if (ev.jhat.value & SDL_HAT_RIGHT)
				return joyButtonToExKey(ev.jbutton.which, 3);

			break;
		case SDL_JOYBUTTONDOWN:
			return joyButtonToExKey(ev.jbutton.which, 16 + ev.jbutton.button);
		}
	}
	
	return 0; // Dummy
}

std::string Gfx::getKeyName(uint32_t key)
{
	if(key < MaxDOSKey)
	{
		return common->texts.keyNames[key];
	}
	else if(key >= JoyKeysStart)
	{
		key -= JoyKeysStart;
		int joyNum = key / MaxJoyButtons;
		key -= joyNum * MaxJoyButtons;
		return "J" + toString(joyNum) + "_" + toString(key);
	}
	
	return "";
}

void Gfx::clearKeys()
{
	std::memset(dosKeys, 0, sizeof(dosKeys));
}

void Gfx::preparePalette(SDL_PixelFormat* format, Color realPal[256], uint32_t (&pal32)[256])
{
	for(int i = 0; i < 256; ++i)
	{
		pal32[i] = SDL_MapRGB(format, realPal[i].r, realPal[i].g, realPal[i].b);		 
	}
}

void Gfx::overlay(
	SDL_PixelFormat* format,
	uint8_t* src, int w, int h, std::size_t srcPitch,
	uint8_t* dest, std::size_t destPitch, int mag)
{
	uint32_t transparent = SDL_MapRGB(format, 255, 0, 255);

	for(int y = 0; y < h; ++y)
	{
		uint8_t* line = src + y*srcPitch;
		int destMagPitch = mag*destPitch;
		uint8_t* destLine = dest + y*destMagPitch;
						
		for(int x = 0; x < w; ++x)
		{
			uint32_t pix = *reinterpret_cast<uint32_t*>(line);
			line += 4;

			if (pix == transparent)
				continue;

			for(int dx = 0; dx < mag; ++dx)
			{
				for(int dy = 0; dy < destMagPitch; dy += destPitch)
				{
					*reinterpret_cast<uint32_t*>(destLine + dy) = pix;
				}
				destLine += 4;
			}
		}
	}
}

void Gfx::menuFlip()
{
	++menuCycles;
	pal = origpal;
	pal.rotateFrom(origpal, 168, 174, menuCycles);
	pal.setWormColours(*settings);
	pal.fade(fadeValue);
	flip();
}

void Gfx::flip()
{
	gvl::rect updateRect;
	Color realPal[256];
	pal.activate(realPal);

	{
		int offsetX, offsetY;
		int mag = fitScreen(back->w, back->h, screenBmp.w, screenBmp.h, offsetX, offsetY, settings->scaleFilter);
		
		gvl::rect newRect(offsetX, offsetY, screenBmp.w * mag, screenBmp.h * mag);
		
		if(mag != prevMag)
		{
			// Clear background if magnification is decreased to
			// avoid leftovers.
			SDL_FillRect(back, 0, 0);
			updateRect = lastUpdateRect | newRect;
		}
		else
			updateRect = newRect;
		prevMag = mag;
		
		std::size_t destPitch = back->pitch;
		std::size_t srcPitch = screenBmp.pitch;
		
		PalIdx* dest = reinterpret_cast<PalIdx*>(back->pixels) + offsetY * destPitch + offsetX * back->format->BytesPerPixel;
		PalIdx* src = screenBmp.pixels;
		
		if(back->format->BitsPerPixel == 32)
		{
			uint32_t pal32[256];

			preparePalette(back->format, realPal, pal32);
		
			scaleDraw(src, 320, 200, srcPitch, dest, destPitch, mag, settings->scaleFilter, pal32);
		}
	}
	
	SDL_Flip(back);
	
	lastUpdateRect = updateRect;
	
	if(settings->screenSync)
	{
		static unsigned int const delay = 14u;
		
		uint32_t wantedTime = lastFrame + delay;

		while(true)
		{
			uint32_t now = SDL_GetTicks();
			if(now >= wantedTime)
				break;
			
			SDL_Delay(wantedTime - now);
		}
		
		lastFrame = SDL_GetTicks();
		while((SDL_GetTicks() - lastFrame) > delay)
			lastFrame += delay;
	}
	else
		SDL_Delay(0);
}

void playChangeSound(Common& common, int change)
{
	if(change > 0)
	{
		sfx.play(common, 25);
	}
	else
	{
		sfx.play(common, 26);
	}
}

void resetLeftRight()
{
	gfx.releaseSDLKey(SDLK_LEFT);
	gfx.releaseSDLKey(SDLK_RIGHT);
}

template<typename T>
void changeVariable(T& var, T change, T min, T max, T scale)
{
	if(change < 0 && var > min)
	{
		var += change * scale;
	}
	if(change > 0 && var < max)
	{
		var += change * scale;
	}
}

struct LevelSelectBehavior : ItemBehavior
{
	LevelSelectBehavior(Common& common)
	: common(common)
	{
	}
	
	int onEnter(Menu& menu, int item)
	{
		sfx.play(common, 27);
		gfx.selectLevel();
		sfx.play(common, 27);
		onUpdate(menu, item);
		return -1;
	}
	
	void onUpdate(Menu& menu, int item)
	{
		if(!gfx.settings->randomLevel)
		{
			menu.items[Settings::SiLevel].value = '"' + getLeaf(gfx.settings->levelFile) + '"';
			menu.items[Settings::SiRegenerateLevel].string = common.texts.reloadLevel; // Not string?
		}
		else
		{
			menu.items[Settings::SiLevel].value = common.texts.random2;
			menu.items[Settings::SiRegenerateLevel].string = common.texts.regenLevel;
		}
	}
	
	Common& common;
};

struct ProfileLoadBehavior : ItemBehavior
{
	ProfileLoadBehavior(Common& common, WormSettings& ws)
	: common(common)
	, ws(ws)
	{
	}
	
	int onEnter(Menu& menu, int item)
	{
		sfx.play(common, 27);
		gfx.selectProfile(ws);
		sfx.play(common, 27);
		menu.updateItems(common);
		return -1;
	}
		
	Common& common;
	WormSettings& ws;
};

struct WeaponOptionsBehavior : ItemBehavior
{
	WeaponOptionsBehavior(Common& common)
	: common(common)
	{
	}

	int onEnter(Menu& menu, int item)
	{
		sfx.play(common, 27);
		gfx.weaponOptions();
		sfx.play(common, 27);
		return -1;
	}

	Common& common;
};

struct PlayerSettingsBehavior : ItemBehavior
{
	PlayerSettingsBehavior(Common& common, int player)
	: player(player)
	, common(common)
	{
	}
	
	int onEnter(Menu& menu, int item)
	{
		sfx.play(common, 27);
		gfx.playerSettings(player);
		return -1;
	}
	
	Common& common;
	int player;
};

ItemBehavior* SettingsMenu::getItemBehavior(Common& common, int item)
{
	switch(item)
	{
		case Settings::SiNamesOnBonuses:
			return new BooleanSwitchBehavior(common, gfx.settings->namesOnBonuses);
		case Settings::SiMap:
			return new BooleanSwitchBehavior(common, gfx.settings->map);
		case Settings::SiRegenerateLevel:
			return new BooleanSwitchBehavior(common, gfx.settings->regenerateLevel);
		case Settings::SiLoadingTimes:
			return new IntegerBehavior(common, gfx.settings->loadingTime, 0, 9999, 1, true);
		case Settings::SiMaxBonuses:
			return new IntegerBehavior(common, gfx.settings->maxBonuses, 0, 99, 1);
		case Settings::SiAmountOfBlood:
		{
			IntegerBehavior* ret = new IntegerBehavior(common, gfx.settings->blood, 0, common.C[BloodLimit], common.C[BloodStepUp], true);
			ret->allowEntry = false;
			return ret;
		}
		
		case Settings::SiLives:
			return new IntegerBehavior(common, gfx.settings->lives, 1, 999, 1);
		case Settings::SiTimeToLose:
		case Settings::SiTimeToWin:
			return new TimeBehavior(common, gfx.settings->timeToLose, 60, 3600, 10);
		case Settings::SiZoneTimeout:
			return new TimeBehavior(common, gfx.settings->zoneTimeout, 10, 3600, 10);
		case Settings::SiFlagsToWin:
			return new IntegerBehavior(common, gfx.settings->flagsToWin, 1, 999, 1);
		
		case Settings::SiLevel:
			return new LevelSelectBehavior(common);
			
		case Settings::SiGameMode:
			return new ArrayEnumBehavior(common, gfx.settings->gameMode, common.texts.gameModes);
		case Settings::SiWeaponOptions:
			return new WeaponOptionsBehavior(common);
		
		default:
			return Menu::getItemBehavior(common, item);
	}
}

struct LevelSort
{
	typedef std::pair<std::string, bool> type;

	bool operator()(type const& a, type const& b) const
	{
		if (a.second == b.second)
			return ciLess(a.first, b.first);
		return a.second > b.second;
	}
};

using std::string;
using std::vector;
using std::pair;
using gvl::shared_ptr;

void Gfx::selectLevel()
{
	FileSelector levSel(*common);

	{
		levSel.fill(lieroEXERoot, [](string const& name, string const& ext) { return ciCompare(ext, "LEV"); });

		shared_ptr<FileNode> random(new FileNode(
					common->texts.random, "", false, &levSel.rootNode));

		random->id = 1;
		levSel.rootNode.children.insert(levSel.rootNode.children.begin(), random);
		levSel.setFolder(levSel.rootNode);
		levSel.select(settings->levelFile);
	}

	do
	{
		std::memcpy(screenBmp.pixels, &frozenScreen[0], frozenScreen.size());
		
		drawBasicMenu();
		
		string title = common->texts.selLevel;
		if (!levSel.currentNode->fullPath.empty())
		{
			title += ' ';
			title += levSel.currentNode->fullPath;
		}
		
		int wid = common->font.getDims(title);

		drawRoundedBox(screenBmp, 178, 20, 0, 7, wid);
		common->font.drawText(screenBmp, title, 180, 21, 50);

		levSel.menu.draw(*common, false);
		
		if (!levSel.process())
			break;
		
		if(testSDLKeyOnce(SDLK_RETURN)
		|| testSDLKeyOnce(SDLK_KP_ENTER))
		{
			sfx.play(*common, 27);
			
			auto* sel = levSel.enter();

			if (sel)
			{
				if (sel->id == 1)
				{
					settings->randomLevel = true;
					settings->levelFile.clear();
				}
				else
				{
					settings->randomLevel = false;
					settings->levelFile = sel->fullPath;
				}
				break;
			}
		}
		
		menuFlip();
		process();
	}
	while(true);
}

void Gfx::selectProfile(WormSettings& ws)
{
	FileSelector profileSel(*common, 28);

	{
		profileSel.fill(lieroEXERoot, [](string const& name, string const& ext) { return ciCompare(ext, "LPF"); });

		profileSel.setFolder(profileSel.rootNode);
		profileSel.select(joinPath(lieroEXERoot, "Profiles"));
	}
	
	do
	{
		std::memcpy(screenBmp.pixels, &frozenScreen[0], frozenScreen.size());

		string title = "Select profile:";
		if (!profileSel.currentNode->fullPath.empty())
		{
			title += ' ';
			title += profileSel.currentNode->fullPath;
		}

		common->font.drawFramedText(screenBmp, title, 28, 20, 50);

		profileSel.menu.draw(*common, false);

		if (!profileSel.process())
			break;

		if(testSDLKeyOnce(SDLK_RETURN)
		|| testSDLKeyOnce(SDLK_KP_ENTER))
		{
			auto* sel = profileSel.enter();

			if (sel)
			{
				ws.loadProfile(sel->fullPath);
				return;
			}
		}
		
		menuFlip();
		process();
	}
	while(true);
	
	return;
}

int Gfx::selectReplay()
{
	FileSelector replaySel(*common, 28);

	{
		replaySel.fill(lieroEXERoot, [](string const& name, string const& ext) { return ciCompare(ext, "LRP"); });

		replaySel.setFolder(replaySel.rootNode);
		replaySel.select(joinPath(lieroEXERoot, "Replays"));
	}
	
	do
	{
		std::memcpy(screenBmp.pixels, &frozenScreen[0], frozenScreen.size());
		
		string title = "Select replay:";
		if (!replaySel.currentNode->fullPath.empty())
		{
			title += ' ';
			title += replaySel.currentNode->fullPath;
		}
		
		common->font.drawFramedText(screenBmp, title, 28, 20, 50);

		replaySel.menu.draw(*common, false);
		
		if (!replaySel.process())
			break;
		
		if(testSDLKeyOnce(SDLK_RETURN)
		|| testSDLKeyOnce(SDLK_KP_ENTER))
		{
			auto* sel = replaySel.enter();

			if (sel)
			{
				std::string fullPath = sel->fullPath + ".lrp";

				{
					// Reset controller before opening the replay, since we may be recording it
					controller.reset();
				
					gvl::stream_ptr replay(new gvl::fstream(std::fopen(fullPath.c_str(), "rb")));

					controller.reset(new ReplayController(common, replay));
				
					return MaReplay;
				}
			}
		}
		menuFlip();
		process();
	}
	while(true);
	
	return -1;
}

void Gfx::selectOptions()
{
	FileSelector optionsSel(*common, 28);

	{
		optionsSel.fill(lieroEXERoot, [](string const& name, string const& ext) {
			return ciCompare(ext, "DAT") && !ciCompare(getLeaf(name), "NAMES.DAT");
		});

		optionsSel.setFolder(optionsSel.rootNode);
	}
	
	do
	{
		std::memcpy(screenBmp.pixels, &frozenScreen[0], frozenScreen.size());
		
		string title = "Select options:";
		if (!optionsSel.currentNode->fullPath.empty())
		{
			title += ' ';
			title += optionsSel.currentNode->fullPath;
		}
		
		common->font.drawFramedText(screenBmp, title, 28, 20, 50);

		optionsSel.menu.draw(*common, false);
		
		if (!optionsSel.process())
			break;
		
		if(testSDLKeyOnce(SDLK_RETURN)
		|| testSDLKeyOnce(SDLK_KP_ENTER))
		{
			auto* sel = optionsSel.enter();

			if (sel)
			{
				gfx.loadSettings(sel->fullPath);
				return;
			}
		}
		menuFlip();
		process();
	}
	while(true);
}

struct WeaponMenu : Menu
{
	WeaponMenu(int x, int y)
	: Menu(x, y)
	{
	}
	
	ItemBehavior* getItemBehavior(Common& common, int item)
	{
		int index = common.weapOrder[item + 1];
		return new ArrayEnumBehavior(common, gfx.settings->weapTable[index], common.texts.weapStates);
	}
};

void Gfx::weaponOptions()
{
	WeaponMenu weaponMenu(179, 28);
			
	weaponMenu.setHeight(14);
	weaponMenu.valueOffsetX = 89;
	
	for(int i = 1; i < 41; ++i)
	{
		int index = common->weapOrder[i];
		weaponMenu.addItem(MenuItem(48, 7, common->weapons[index].name));
	}
	
	weaponMenu.moveToFirstVisible();
	weaponMenu.updateItems(*common);
	
	while(true)
	{
		std::memcpy(gfx.screenBmp.pixels, &frozenScreen[0], frozenScreen.size());
		
		drawBasicMenu();
		
		drawRoundedBox(screenBmp, 179, 20, 0, 7, common->font.getDims(common->texts.weapon));
		drawRoundedBox(screenBmp, 249, 20, 0, 7, common->font.getDims(common->texts.availability));
		
		common->font.drawText(screenBmp, common->texts.weapon, 181, 21, 50);
		common->font.drawText(screenBmp, common->texts.availability, 251, 21, 50);
		
		weaponMenu.draw(*common, false);
						
		if(testSDLKeyOnce(SDLK_UP))
		{
			sfx.play(*common, 26);
			weaponMenu.movement(-1);
		}
		
		if(testSDLKeyOnce(SDLK_DOWN))
		{
			sfx.play(*common, 25);
			weaponMenu.movement(1);
		}
		
		if(testSDLKeyOnce(SDLK_LEFT))
		{
			weaponMenu.onLeftRight(*common, -1);
		}
		if(testSDLKeyOnce(SDLK_RIGHT))
		{
			weaponMenu.onLeftRight(*common, 1);
		}
		
		if(settings->extensions)
		{
			if(testSDLKeyOnce(SDLK_PAGEUP))
			{
				sfx.play(*common, 26);
				
				weaponMenu.movementPage(-1);
			}
			
			if(testSDLKeyOnce(SDLK_PAGEDOWN))
			{
				sfx.play(*common, 25);
				
				weaponMenu.movementPage(1);
			}
		}

		weaponMenu.onKeys(gfx.keyBuf, gfx.keyBufPtr);

		menuFlip();
		process();
		
		if(testSDLKeyOnce(SDLK_ESCAPE))
		{
			int count = 0;
			
			for(int i = 0; i < 40; ++i)
			{
				if(settings->weapTable[i] == 0)
					++count;
			}
				
			if(count > 0)
				break; // Enough weapons available
				
			infoBox(common->texts.noWeaps, 223, 68, false);
		}
	}
}

void Gfx::infoBox(std::string const& text, int x, int y, bool clearScreen)
{
	static int const bgColor = 0;
	
	if(clearScreen)
	{
		pal = common->exepal;
		fill(screenBmp, bgColor);
	}
	
	int height;
	int width = common->font.getDims(text, &height);
	
	int cx = x - width/2 - 2;
	int cy = y - height/2 - 2;
	
	drawRoundedBox(screenBmp, cx, cy, 0, height+1, width+1);
	common->font.drawText(screenBmp, text, cx+2, cy+2, 6);
	
	flip();
	process();
	
	waitForKey();
	clearKeys();
	
	if(clearScreen)
		fill(screenBmp, bgColor);
}

bool Gfx::inputString(std::string& dest, std::size_t maxLen, int x, int y, int (*filter)(int), std::string const& prefix, bool centered)
{
	std::string buffer = dest;
	
	while(true)
	{
		std::string str = prefix + buffer + '_';
		
		Font& font = common->font;
		
		int width = font.getDims(str);
		
		int adjust = centered ? width/2 : 0;
		
		int clrX = x - 10 - adjust;
		
		int offset = clrX + y*320; // TODO: Unhardcode 320
		
		blitImageNoKeyColour(screenBmp, &frozenScreen[offset], clrX, y, clrX + 10 + width, 8, 320);
		
		drawRoundedBox(screenBmp, x - 2 - adjust, y, 0, 7, width);
		
		font.drawText(screenBmp, str, x - adjust, y + 1, 50);
		flip();
		SDL_keysym key(waitForKey());
		
		switch(key.sym)
		{
		case SDLK_BACKSPACE:
			if(!buffer.empty())
			{
				buffer.erase(buffer.size() - 1);
			}
		break;
		
		case SDLK_RETURN:
		case SDLK_KP_ENTER:
			dest = buffer;
			sfx.play(*common, 27);
			clearKeys();
			return true;
			
		case SDLK_ESCAPE:
			clearKeys();
			return false;
			
		default:
			int k = unicodeToDOS(key.unicode);
			if(k
			&& buffer.size() < maxLen
			&& (
			    !filter
			 || (k = filter(k))))
			{
				buffer += char(k);
			}
		}
	}
}

int filterDigits(int k)
{
	return std::isdigit(k) ? k : 0;
}

void Gfx::inputInteger(int& dest, int min, int max, std::size_t maxLen, int x, int y)
{
	std::string str(toString(dest));
	
	if(inputString(str, maxLen, x, y, filterDigits)
	&& !str.empty())
	{
		dest = std::atoi(str.c_str());
		if(dest < min)
			dest = min;
		else if(dest > max)
			dest = max;
	}
}

void PlayerMenu::drawItemOverlay(Common& common, int item, int x, int y, bool selected, bool disabled)
{
	if(item >= 2 && item <= 4) //Color settings
	{
		int rgbcol = item - 2;

		if(selected)
		{
			drawRoundedBox(gfx.screenBmp, x + 24, y, 168, 7, ws->rgb[rgbcol] - 1);
		}
		else // CE98
		{
			drawRoundedBox(gfx.screenBmp, x + 24, y, 0, 7, ws->rgb[rgbcol] - 1);
		}
		
		fillRect(gfx.screenBmp, x + 25, y + 1, ws->rgb[rgbcol], 5, ws->color);
	} // CED9
}



ItemBehavior* PlayerMenu::getItemBehavior(Common& common, int item)
{
	switch(item)
	{
		case 0:
			return new WormNameBehavior(common, *ws);
		case 1:
		{
			auto* b = new IntegerBehavior(common, ws->health, 1, 10000, 1, true);
			b->scrollInterval = 4;
			return b;
		}

		case 2:
		case 3:
		case 4:
		{
			auto* b = new IntegerBehavior(common, ws->rgb[item - 2], 0, 63, 1, false);
			b->scrollInterval = 4;
			return b;
		}
			
		case 5: // D2AB
		case 6:
		case 7:
		case 8:
		case 9:
		case 10:
		case 11:
			return new KeyBehavior(common, ws->controls[item - 5], ws->controlsEx[item - 5], gfx.settings->extensions );
		
		case 12: // Controls Extension
			return new KeyBehavior(common, ws->controlsEx[item - 5], ws->controlsEx[item - 5], gfx.settings->extensions );

			
		case 13: // Controller
		{
			// Controller cannot be changed with Enter
			return new ArrayEnumBehavior(common, ws->controller, common.texts.controllers, true);
		}
		
		case 14: // Save profile
			return new ProfileSaveBehavior(common, *ws, false);
			
		case 15: // Save profile as
			return new ProfileSaveBehavior(common, *ws, true);
			
		case 16:
			return new ProfileLoadBehavior(common, *ws);
			
		default:
			return Menu::getItemBehavior(common, item);
	}
}

void Gfx::playerSettings(int player)
{
	playerMenu.ws = settings->wormSettings[player];
	
	playerMenu.updateItems(*common);
	playerMenu.moveToFirstVisible();
	
	curMenu = &playerMenu;
	return;
}

void Gfx::mainLoop()
{
	controller.reset(new LocalController(common, settings));
	
	{
		Level newLevel(*common);
		newLevel.generateFromSettings(*common, *settings, rand);
		controller->swapLevel(newLevel);
	}
	
	controller->currentGame()->focus(*this);
	// TODO: Unfocus game when necessary
	
	while(true)
	{
		clear();
		controller->draw(*this);
		
		mainMenu.setVisibility(0, controller->running());
		int selection = menuLoop();
		
		if(selection == MaNewGame)
		{
			std::auto_ptr<Controller> newController(new LocalController(common, settings));
			
			Level* oldLevel = controller->currentLevel();
			
			if(oldLevel
			&& !settings->regenerateLevel
			&& settings->randomLevel == oldLevel->oldRandomLevel
			&& settings->levelFile == oldLevel->oldLevelFile)
			{
				// Take level and palette from old game
				newController->swapLevel(*oldLevel);
			}
			else
			{
				Level newLevel(*common);
				newLevel.generateFromSettings(*common, *settings, rand);
				newController->swapLevel(newLevel);
			}
			
			controller = newController;
		}
		else if(selection == MaResumeGame)
		{
			
		}
		else if(selection == MaQuit) // QUIT TO OS
		{
			break;
		}
		else if(selection == MaReplay)
		{
			//controller.reset(new ReplayController(common/*, settings*/));
		}
		
		controller->focus();
		
		while(true)
		{
			if(!controller->process())
				break;
			clear();
			controller->draw(*this);
			
			flip();
			process(controller.get());
		}
		
		controller->unfocus();
		
		clearKeys();
		
		
		/*
		game->shutDown = false;
	
		do
		{
			game->processFrame();
			clear();
			game->draw();
			
			flip();
			process(game.get());
		}
		while(fadeValue > 0);*/
		
		
	}

	controller.reset();
}

void Gfx::saveSettings(std::string const& path)
{
	settingsFile = path;
	settings->save(settingsFile + ".DAT", rand);
}

bool Gfx::loadSettings(std::string const& path)
{
	settingsFile = path;
	settings.reset(new Settings);
	return settings->load(settingsFile + ".DAT", rand);
}

void Gfx::drawBasicMenu(/*int curSel*/)
{
	std::memcpy(screenBmp.pixels, &frozenScreen[0], frozenScreen.size());
#if 0	
	common->font.drawText(screenBmp, common->texts.saveoptions, 36, 54+20, 0);
	common->font.drawText(screenBmp, common->texts.loadoptions, 36, 61+20, 0);
	
	common->font.drawText(screenBmp, common->texts.saveoptions, 36, 53+20, 10);
	common->font.drawText(screenBmp, common->texts.loadoptions, 36, 60+20, 10);
	

	if(settingsFile.empty())
	{
		common->font.drawText(screenBmp, common->texts.curOptNoFile, 36, 46+20, 0);
		common->font.drawText(screenBmp, common->texts.curOptNoFile, 35, 45+20, 147);
	}
	else
	{
		common->font.drawText(screenBmp, common->texts.curOpt + settingsFile, 36, 46+20, 0);
		common->font.drawText(screenBmp, common->texts.curOpt + settingsFile, 35, 45+20, 147);
	}
#endif
	
	mainMenu.draw(*common, curMenu != &mainMenu);
}

int upperCaseOnly(int k)
{
	k = std::toupper(k);
	
	if((k >= 'A' && k <= 'Z')
	|| (k == 0x8f || k == 0x8e || k == 0x99) // � �and �
	|| (k >= '0' && k <= '9'))
		return k;
		
	return 0;
}

void Gfx::openHiddenMenu()
{
	if (curMenu == &hiddenMenu)
		return;
	curMenu = &hiddenMenu;
	curMenu->updateItems(*common);
	curMenu->moveToFirstVisible();
}

int Gfx::menuLoop()
{
	std::memset(pal.entries, 0, sizeof(pal.entries));
	flip();
	process();
	
	fillRect(screenBmp, 0, 151, 160, 7, 0);
	common->font.drawText(screenBmp, common->texts.copyright2, 2, 152, 19);
	
	mainMenu.moveToFirstVisible();
	settingsMenu.moveToFirstVisible();
	settingsMenu.updateItems(*common);
	
	fadeValue = 0;
	curMenu = &mainMenu;

	std::memcpy(&frozenScreen[0], screenBmp.pixels, frozenScreen.size());

	updateSettingsMenu();
	
	menuCycles = 0;
	int selected = -1;
		
	do
	{
		drawBasicMenu();
		
		if(curMenu == &mainMenu)
			settingsMenu.draw(*common, true);
		else
			curMenu->draw(*common, false);
		
		if(testSDLKeyOnce(SDLK_ESCAPE))
		{
			if(curMenu == &mainMenu)
				mainMenu.moveTo(MaQuit);
			else
				curMenu = &mainMenu;
		}
		
		if(testSDLKeyOnce(SDLK_UP))
		{
			sfx.play(*common, 26);
			curMenu->movement(-1);
		}
		
		if(testSDLKeyOnce(SDLK_DOWN))
		{
			sfx.play(*common, 25);
			curMenu->movement(1);
		}

		if(testSDLKeyOnce(SDLK_RETURN)
		|| testSDLKeyOnce(SDLK_KP_ENTER))
		{
			if(curMenu == &mainMenu)
			{
				sfx.play(*common, 27);
				
				if(mainMenu.selection() == MaSettings)
				{
					curMenu = &settingsMenu; // Go into settings menu
				}
				else if (mainMenu.selection() == MaPlayer1Settings || mainMenu.selection() == MaPlayer2Settings)
				{
					playerSettings(mainMenu.selection() - MaPlayer1Settings);
				}
				else if (mainMenu.selection() == MaAdvanced)
				{
					openHiddenMenu();
				}
				else
				{
					curMenu = &mainMenu;
					selected = mainMenu.selection();
				}
			}
			else if(curMenu == &settingsMenu)
			{
				settingsMenu.onEnter(*common);
				updateSettingsMenu();
			}
			else
			{
				selected = curMenu->onEnter(*common);
			}
		}
		
		if(testSDLKeyOnce(SDLK_F1))
		{
			openHiddenMenu();
		}

		if(testSDLKeyOnce(SDLK_1))
		{
			playerSettings(0);
		}
		if(testSDLKeyOnce(SDLK_2))
		{
			playerSettings(1);
		}
		
#if 0
		if(testSDLKeyOnce(SDLK_s)) // TODO: Check for the real 's' here?
		{
			if(inputString(settingsFile, 8, 35, 65, upperCaseOnly, "Filename: ", false))
			{
				saveSettings();
			}
		}
		
		if(testSDLKeyOnce(SDLK_l)) // TODO: Check if inputString should make a sound even when loading fails
		{
			while(inputString(settingsFile, 8, 35, 65, upperCaseOnly, "Filename: ", false))
			{
				if(loadSettings())
				{
					updateSettingsMenu();
					settingsMenu.updateItems(*common);
					break;
				}
			}
		}
#endif

		if(curMenu == &settingsMenu)
		{
			if(testSDLKey(SDLK_LEFT))
			{
				//settingLeftRight(-1, settingsMenu.selection());
				if(!settingsMenu.onLeftRight(*common, -1))
					resetLeftRight();
				updateSettingsMenu();
			} // EDAE
			if(testSDLKey(SDLK_RIGHT))
			{
				//settingLeftRight(1, settingsMenu.selection());
				if(!settingsMenu.onLeftRight(*common, 1))
					resetLeftRight();
				updateSettingsMenu();
			} // EDBF
		}
		else // if(curMenu == &playerMenu)
		{
			if(testSDLKey(SDLK_LEFT))
			{
				if(!curMenu->onLeftRight(*common, -1))
					resetLeftRight();
			}
			if(testSDLKey(SDLK_RIGHT))
			{
				if(!curMenu->onLeftRight(*common, 1))
					resetLeftRight();
			}
		}
		
		if(testSDLKeyOnce(SDLK_PAGEUP))
		{
			sfx.play(*common, 26);
				
			curMenu->movementPage(-1);
		}
			
		if(testSDLKeyOnce(SDLK_PAGEDOWN))
		{
			sfx.play(*common, 25);
				
			curMenu->movementPage(1);
		}

		/*
		// TODO: Fix hidden menu palette
		if(curMenu != &hiddenMenu)
		{
			//origpal.setWormColours(*settings);
			//origpal.rotate(168, 174);
			pal = origpal;
			pal.setWormColours(*settings);
			pal.rotateFrom(origpal, 168, 174, menuCycles);
		}
		else
		{
			pal = common->exepal;
		}
		*/

		if(fadeValue < 32)
		{
			fadeValue += 1;
			//pal.fade(fadeValue);
		}
		
		menuFlip();
		process();
	}
	while(selected < 0);

	for(fadeValue = 32; fadeValue > 0; --fadeValue)
	{
		menuFlip();
		//pal = origpal;
		//pal.fade(fadeValue);
		//flip(); // TODO: We should just screen sync and set the palette here
	} // EE36
	
	return selected;
}


