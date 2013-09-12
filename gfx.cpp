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

#include <gvl/io2/fstream.hpp>

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
	
	int onEnter(Menu& menu, MenuItem& item)
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
	
	void onUpdate(Menu& menu, MenuItem& item)
	{
		item.value = gfx.getKeyName(extended ? keyEx : key);
		item.hasValue = true;
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
	
	int onEnter(Menu& menu, MenuItem& item)
	{
		sfx.play(common, 27);
		
		ws.randomName = false;

		int x, y;
		if(!menu.itemPosition(item, x, y))
			return -1;
			
		x += menu.valueOffsetX + 2;

		gfx.inputString(ws.name, 20, x, y);
		
		if(ws.name.empty())
		{
			Settings::generateName(ws, gfx.rand);
		}
		sfx.play(common, 27);
		onUpdate(menu, item);
		return -1;
	}
	
	void onUpdate(Menu& menu, MenuItem& item)
	{
		item.value = ws.name;
		item.hasValue = true;
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
	
	int onEnter(Menu& menu, MenuItem& item)
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
	
	void onUpdate(Menu& menu, MenuItem& item)
	{
		if(!saveAs)
		{
			item.visible = !ws.profilePath.empty();
		}
	}
	
	Common& common;
	WormSettings& ws;
	bool saveAs;
};

struct ProfileLoadedBehavior : ItemBehavior
{
	ProfileLoadedBehavior(Common& common, WormSettings& ws)
	: common(common)
	, ws(ws)
	{
	}
	
	void onUpdate(Menu& menu, MenuItem& item)
	{
		item.value = getBasename(getLeaf(ws.profilePath));
		item.hasValue = true;
		item.visible = !ws.profilePath.empty();
	}
	
	Common& common;
	WormSettings& ws;
};

struct WeaponEnumBehavior : EnumBehavior
{
	WeaponEnumBehavior(Common& common, uint32_t& v)
	: EnumBehavior(common, v, 1, 40, false)
	{
	}
		
	void onUpdate(Menu& menu, MenuItem& item)
	{
		item.value = common.weapons[common.weapOrder[v]].name;
		item.hasValue = true;
	}
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
	hiddenMenu.addItem(MenuItem(48, 7, "FULLSCREEN (F5)", HiddenMenu::Fullscreen));
	hiddenMenu.addItem(MenuItem(48, 7, "DOUBLE SIZE (F6)", HiddenMenu::DoubleRes));
	hiddenMenu.addItem(MenuItem(48, 7, "SET FULLSCREEN WIDTH", HiddenMenu::FullscreenW));
	hiddenMenu.addItem(MenuItem(48, 7, "SET FULLSCREEN HEIGHT", HiddenMenu::FullscreenH));
	hiddenMenu.addItem(MenuItem(48, 7, "POWERLEVEL PALETTES", HiddenMenu::LoadPowerLevels));
	hiddenMenu.addItem(MenuItem(48, 7, "SHADOWS", HiddenMenu::Shadows));
	hiddenMenu.addItem(MenuItem(48, 7, "SCALING FILTER", HiddenMenu::ScalingFilter));
	hiddenMenu.addItem(MenuItem(48, 7, "SCREEN SYNC.", HiddenMenu::ScreenSync));
	hiddenMenu.addItem(MenuItem(48, 7, "AUTO-RECORD REPLAYS", HiddenMenu::RecordReplays));
	hiddenMenu.addItem(MenuItem(48, 7, "AI FRAMES", HiddenMenu::AiFrames));
	hiddenMenu.addItem(MenuItem(48, 7, "AI MUTATIONS", HiddenMenu::AiMutations));
	hiddenMenu.addItem(MenuItem(48, 7, "PALETTE", HiddenMenu::PaletteSelect));
	hiddenMenu.addItem(MenuItem(48, 7, "BOT WEAPONS", HiddenMenu::SelectBotWeapons));

	playerMenu.addItem(MenuItem(3, 7, "PROFILE LOADED", PlayerMenu::PlLoadedProfile));
	playerMenu.addItem(MenuItem(3, 7, "SAVE PROFILE", PlayerMenu::PlSaveProfile));
	playerMenu.addItem(MenuItem(3, 7, "SAVE PROFILE AS...", PlayerMenu::PlSaveProfileAs));
	playerMenu.addItem(MenuItem(3, 7, "LOAD PROFILE", PlayerMenu::PlLoadProfile));
	playerMenu.addItem(MenuItem(48, 7, "NAME", PlayerMenu::PlName));
	playerMenu.addItem(MenuItem(48, 7, "HEALTH", PlayerMenu::PlHealth));
	playerMenu.addItem(MenuItem(48, 7, "Red", PlayerMenu::PlRed));
	playerMenu.addItem(MenuItem(48, 7, "Green", PlayerMenu::PlGreen));
	playerMenu.addItem(MenuItem(48, 7, "Blue", PlayerMenu::PlBlue));
	playerMenu.addItem(MenuItem(48, 7, "AIM UP", PlayerMenu::PlUp));
	playerMenu.addItem(MenuItem(48, 7, "AIM DOWN", PlayerMenu::PlDown));
	playerMenu.addItem(MenuItem(48, 7, "MOVE LEFT", PlayerMenu::PlLeft));
	playerMenu.addItem(MenuItem(48, 7, "MOVE RIGHT", PlayerMenu::PlRight));
	playerMenu.addItem(MenuItem(48, 7, "FIRE", PlayerMenu::PlFire));
	playerMenu.addItem(MenuItem(48, 7, "CHANGE", PlayerMenu::PlChange));
	playerMenu.addItem(MenuItem(48, 7, "JUMP", PlayerMenu::PlJump));
	playerMenu.addItem(MenuItem(48, 7, "DIG", PlayerMenu::PlDig));

	for (int i = 0; i < 5; ++i)
		playerMenu.addItem(MenuItem(48, 7, std::string("WEAPON ") + (char)(i + '1'), PlayerMenu::PlWeap0 + i));

	playerMenu.addItem(MenuItem(48, 7, "CONTROLLER", PlayerMenu::PlController));

	settingsMenu.addItem(MenuItem(48, 7, "GAME MODE", SettingsMenu::SiGameMode));
	settingsMenu.addItem(MenuItem(48, 7, "TIME TO LOSE", SettingsMenu::SiTimeToLose));
	settingsMenu.addItem(MenuItem(48, 7, "TIME TO WIN", SettingsMenu::SiTimeToWin));
	settingsMenu.addItem(MenuItem(48, 7, "ZONE TIMEOUT", SettingsMenu::SiZoneTimeout));
	settingsMenu.addItem(MenuItem(48, 7, "FLAGS TO WIN", SettingsMenu::SiFlagsToWin));
	settingsMenu.addItem(MenuItem(48, 7, "LIVES", SettingsMenu::SiLives));
	settingsMenu.addItem(MenuItem(48, 7, "LEVEL", SettingsMenu::SiLevel));
	settingsMenu.addItem(MenuItem(48, 7, "LOADING TIMES", SettingsMenu::SiLoadingTimes));
	settingsMenu.addItem(MenuItem(48, 7, "WEAPON OPTIONS", SettingsMenu::SiWeaponOptions));
	settingsMenu.addItem(MenuItem(48, 7, "MAX BONUSES", SettingsMenu::SiMaxBonuses));
	settingsMenu.addItem(MenuItem(48, 7, "NAMES ON BONUSES", SettingsMenu::SiNamesOnBonuses));
	settingsMenu.addItem(MenuItem(48, 7, "MAP", SettingsMenu::SiMap));
	settingsMenu.addItem(MenuItem(48, 7, "AMOUNT OF BLOOD", SettingsMenu::SiAmountOfBlood));
	settingsMenu.addItem(MenuItem(48, 7, "LOAD+CHANGE", SettingsMenu::LoadChange));
	settingsMenu.addItem(MenuItem(48, 7, "REGENERATE LEVEL", SettingsMenu::SiRegenerateLevel));
	settingsMenu.addItem(MenuItem(48, 7, "SAVE SETUP AS...", SettingsMenu::SaveOptions));
	settingsMenu.addItem(MenuItem(48, 7, "LOAD SETUP", SettingsMenu::LoadOptions));

	mainMenu.addItem(MenuItem(10, 10, "", MainMenu::MaResumeGame)); // string set in menuLoop
	mainMenu.addItem(MenuItem(10, 10, "", MainMenu::MaNewGame)); // string set in menuLoop
	mainMenu.addItem(MenuItem(48, 48, "OPTIONS (F2)", MainMenu::MaAdvanced));
	mainMenu.addItem(MenuItem(48, 48, "REPLAYS (F3)", MainMenu::MaReplays));
	mainMenu.addItem(MenuItem(6, 6, "QUIT TO OS (F10)", MainMenu::MaQuit));
	mainMenu.addItem(MenuItem::space());
	mainMenu.addItem(MenuItem(48, 48, "LEFT PLAYER (1)", MainMenu::MaPlayer1Settings));
	mainMenu.addItem(MenuItem(48, 48, "RIGHT PLAYER (2)", MainMenu::MaPlayer2Settings));
	mainMenu.addItem(MenuItem(48, 48, "MATCH SETUP (3)", MainMenu::MaSettings));

	settingsMenu.valueOffsetX = 100;
	playerMenu.valueOffsetX = 95;
	hiddenMenu.valueOffsetX = 120;
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

			if(s == SDLK_F5)
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

		case SDL_ACTIVEEVENT:
		{
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

void Gfx::menuFlip(bool quitting)
{
	if (fadeValue < 32 && !quitting)
		++fadeValue;

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

struct ProfileLoadBehavior : ItemBehavior
{
	ProfileLoadBehavior(Common& common, WormSettings& ws)
	: common(common)
	, ws(ws)
	{
	}
	
	int onEnter(Menu& menu, MenuItem& item)
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



struct PlayerSettingsBehavior : ItemBehavior
{
	PlayerSettingsBehavior(Common& common, int player)
	: player(player)
	, common(common)
	{
	}
	
	int onEnter(Menu& menu, MenuItem& item)
	{
		sfx.play(common, 27);
		gfx.playerSettings(player);
		return -1;
	}
	
	Common& common;
	int player;
};

struct LevelSelectBehavior : ItemBehavior
{
	LevelSelectBehavior(Common& common)
	: common(common)
	{
	}
	
	int onEnter(Menu& menu, MenuItem& item)
	{
		sfx.play(common, 27);
		gfx.selectLevel();
		sfx.play(common, 27);
		onUpdate(menu, item);
		return -1;
	}
	
	void onUpdate(Menu& menu, MenuItem& item)
	{
		item.hasValue = true;
		if(!gfx.settings->randomLevel)
		{
			item.value = '"' + getBasename(getLeaf(gfx.settings->levelFile)) + '"';
			menu.itemFromId(SettingsMenu::SiRegenerateLevel)->string = LS(ReloadLevel); // Not string?
		}
		else
		{
			item.value = LS(Random2);
			menu.itemFromId(SettingsMenu::SiRegenerateLevel)->string = LS(RegenLevel);
		}
	}
	
	Common& common;
};

struct WeaponOptionsBehavior : ItemBehavior
{
	WeaponOptionsBehavior(Common& common)
	: common(common)
	{
	}

	int onEnter(Menu& menu, MenuItem& item)
	{
		sfx.play(common, 27);
		gfx.weaponOptions();
		sfx.play(common, 27);
		return -1;
	}

	Common& common;
};

struct OptionsSaveBehavior : ItemBehavior
{
	OptionsSaveBehavior(Common& common)
	: common(common)
	{
	}
	
	int onEnter(Menu& menu, MenuItem& item)
	{
		sfx.play(common, 27);
		
		int x, y;
		if(!menu.itemPosition(item, x, y))
			return -1;
			
		x += menu.valueOffsetX + 2;
		
		std::string name = gfx.settingsFile;
		if(gfx.inputString(name, 30, x, y) && !name.empty())
		{
			gfx.saveSettings(name);
		}
				
		sfx.play(common, 27);
		
		onUpdate(menu, item);
		return -1;
	}
	
	void onUpdate(Menu& menu, MenuItem& item)
	{
		item.value = getBasename(getLeaf(gfx.settingsFile));
		item.hasValue = true;
	}
	
	Common& common;
};

struct OptionsSelectBehavior : ItemBehavior
{
	OptionsSelectBehavior(Common& common)
	: common(common)
	{
	}

	int onEnter(Menu& menu, MenuItem& item)
	{
		sfx.play(common, 27);
		gfx.selectOptions();
		sfx.play(common, 27);
		menu.updateItems(common);
		return -1;
	}

	Common& common;
};

ItemBehavior* SettingsMenu::getItemBehavior(Common& common, MenuItem& item)
{
	switch(item.id)
	{
		case SiNamesOnBonuses:
			return new BooleanSwitchBehavior(common, gfx.settings->namesOnBonuses);
		case SiMap:
			return new BooleanSwitchBehavior(common, gfx.settings->map);
		case SiRegenerateLevel:
			return new BooleanSwitchBehavior(common, gfx.settings->regenerateLevel);
		case SiLoadingTimes:
			return new IntegerBehavior(common, gfx.settings->loadingTime, 0, 9999, 1, true);
		case SiMaxBonuses:
			return new IntegerBehavior(common, gfx.settings->maxBonuses, 0, 99, 1);
		case SiAmountOfBlood:
		{
			IntegerBehavior* ret = new IntegerBehavior(common, gfx.settings->blood, 0, LC(BloodLimit), LC(BloodStepUp), true);
			ret->allowEntry = false;
			return ret;
		}
		
		case SiLives:
			return new IntegerBehavior(common, gfx.settings->lives, 1, 999, 1);
		case SiTimeToLose:
		case SiTimeToWin:
			return new TimeBehavior(common, gfx.settings->timeToLose, 60, 3600, 10);
		case SiZoneTimeout:
			return new TimeBehavior(common, gfx.settings->zoneTimeout, 10, 3600, 10);
		case SiFlagsToWin:
			return new IntegerBehavior(common, gfx.settings->flagsToWin, 1, 999, 1);
		
		case SiLevel:
			return new LevelSelectBehavior(common);
			
		case SiGameMode:
			return new ArrayEnumBehavior(common, gfx.settings->gameMode, common.texts.gameModes);
		case SiWeaponOptions:
			return new WeaponOptionsBehavior(common);
		case LoadOptions:
			return new OptionsSelectBehavior(common);
		case SaveOptions:
			return new OptionsSaveBehavior(common);
		case LoadChange:
			return new BooleanSwitchBehavior(common, gfx.settings->loadChange);
		default:
			return Menu::getItemBehavior(common, item);
	}
}

void SettingsMenu::onUpdate()
{
	setVisibility(SiLives, false);
	setVisibility(SiTimeToLose, false);
	setVisibility(SiTimeToWin, false);
	setVisibility(SiZoneTimeout, false);
	setVisibility(SiFlagsToWin, false);
	
	switch(gfx.settings->gameMode)
	{
		case Settings::GMKillEmAll:
			setVisibility(SiLives, true);
		break;
		
		case Settings::GMGameOfTag:
			setVisibility(SiTimeToLose, true);
		break;

		case Settings::GMHoldazone:
			setVisibility(SiTimeToWin, true);
			setVisibility(SiZoneTimeout, true);
		break;
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
	Common& common = *this->common;
	FileSelector levSel(common);

	{
		levSel.fill(lieroEXERoot, [](string const& name, string const& ext) { return ciCompare(ext, "LEV"); });

		shared_ptr<FileNode> random(new FileNode(
					LS(Random), "", false, &levSel.rootNode));

		random->id = 1;
		levSel.rootNode.children.insert(levSel.rootNode.children.begin(), random);
		levSel.setFolder(levSel.rootNode);
		levSel.select(settings->levelFile);
	}

	do
	{
		std::memcpy(screenBmp.pixels, &frozenScreen[0], frozenScreen.size());
		
		string title = LS(SelLevel);
		if (!levSel.currentNode->fullPath.empty())
		{
			title += ' ';
			title += levSel.currentNode->fullPath;
		}
		
		int wid = common.font.getDims(title);

		drawRoundedBox(screenBmp, 178, 20, 0, 7, wid);
		common.font.drawText(screenBmp, title, 180, 21, 50);

		levSel.draw();
		
		if (!levSel.process())
			break;
		
		if(testSDLKeyOnce(SDLK_RETURN)
		|| testSDLKeyOnce(SDLK_KP_ENTER))
		{
			sfx.play(common, 27);
			
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

		common->font.drawFramedText(screenBmp, title, 178, 20, 50);

		profileSel.draw();

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
		if (prevSelectedReplayPath.empty()
		  || !replaySel.select(prevSelectedReplayPath))
		{
			replaySel.select(joinPath(lieroEXERoot, "Replays"));
		}
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
		
		common->font.drawFramedText(screenBmp, title, 178, 20, 50);

		replaySel.draw();
		
		if (!replaySel.process())
			break;
		
		if(testSDLKeyOnce(SDLK_RETURN)
		|| testSDLKeyOnce(SDLK_KP_ENTER))
		{
			auto* sel = replaySel.enter();

			if (sel)
			{
				prevSelectedReplayPath = sel->fullPath;

				// Reset controller before opening the replay, since we may be recording it
				controller.reset();
				
				auto replay(
					gvl::to_source(new gvl::file_bucket_source(sel->fullPath.c_str(), "rb")));

				controller.reset(new ReplayController(common, replay));
				
				return MainMenu::MaReplay;
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
		
		common->font.drawFramedText(screenBmp, title, 178, 20, 50);

		optionsSel.draw();
		
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
	
	ItemBehavior* getItemBehavior(Common& common, MenuItem& item)
	{
		int index = common.weapOrder[item.id + 1];
		return new ArrayEnumBehavior(common, gfx.settings->weapTable[index], common.texts.weapStates);
	}
};

void Gfx::weaponOptions()
{
	Common& common = *this->common;
	WeaponMenu weaponMenu(179, 28);
			
	weaponMenu.setHeight(14);
	weaponMenu.valueOffsetX = 89;
	
	for(int i = 1; i < 41; ++i)
	{
		int index = common.weapOrder[i];
		weaponMenu.addItem(MenuItem(48, 7, common.weapons[index].name, i - 1));
	}
	
	weaponMenu.moveToFirstVisible();
	weaponMenu.updateItems(common);
	
	while(true)
	{
		std::memcpy(gfx.screenBmp.pixels, &frozenScreen[0], frozenScreen.size());
		
		drawBasicMenu();
		
		drawRoundedBox(screenBmp, 179, 20, 0, 7, common.font.getDims(LS(Weapon)));
		drawRoundedBox(screenBmp, 249, 20, 0, 7, common.font.getDims(LS(Availability)));
		
		common.font.drawText(screenBmp, LS(Weapon), 181, 21, 50);
		common.font.drawText(screenBmp, LS(Availability), 251, 21, 50);
		
		weaponMenu.draw(common, false);
						
		if(testSDLKeyOnce(SDLK_UP))
		{
			sfx.play(common, 26);
			weaponMenu.movement(-1);
		}
		
		if(testSDLKeyOnce(SDLK_DOWN))
		{
			sfx.play(common, 25);
			weaponMenu.movement(1);
		}
		
		if(testSDLKeyOnce(SDLK_LEFT))
		{
			weaponMenu.onLeftRight(common, -1);
		}
		if(testSDLKeyOnce(SDLK_RIGHT))
		{
			weaponMenu.onLeftRight(common, 1);
		}
		
		if(settings->extensions)
		{
			if(testSDLKeyOnce(SDLK_PAGEUP))
			{
				sfx.play(common, 26);
				
				weaponMenu.movementPage(-1);
			}
			
			if(testSDLKeyOnce(SDLK_PAGEDOWN))
			{
				sfx.play(common, 25);
				
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
				
			infoBox(LS(NoWeaps), 223, 68, false);
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

void PlayerMenu::drawItemOverlay(Common& common, MenuItem& item, int x, int y, bool selected, bool disabled)
{
	if(item.id >= PlayerMenu::PlRed && item.id <= PlayerMenu::PlBlue) //Color settings
	{
		int rgbcol = item.id - PlayerMenu::PlRed;

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

ItemBehavior* PlayerMenu::getItemBehavior(Common& common, MenuItem& item)
{
	if (item.id >= PlWeap0 && item.id < PlWeap0 + 5)
		return new WeaponEnumBehavior(common, ws->weapons[item.id - PlWeap0]);

	switch(item.id)
	{
		case PlName:
			return new WormNameBehavior(common, *ws);
		case PlHealth:
		{
			auto* b = new IntegerBehavior(common, ws->health, 1, 10000, 1, true);
			b->scrollInterval = 4;
			return b;
		}

		case PlRed:
		case PlGreen:
		case PlBlue:
		{
			auto* b = new IntegerBehavior(common, ws->rgb[item.id - PlRed], 0, 63, 1, false);
			b->scrollInterval = 4;
			return b;
		}
			
		case PlUp: // D2AB
		case PlDown:
		case PlLeft:
		case PlRight:
		case PlFire:
		case PlChange:
		case PlJump:
			return new KeyBehavior(common, ws->controls[item.id - PlUp], ws->controlsEx[item.id - PlUp], gfx.settings->extensions);
		
		case PlDig: // Controls Extension
			return new KeyBehavior(common, ws->controlsEx[item.id - PlUp], ws->controlsEx[item.id - PlUp], gfx.settings->extensions);

			
		case PlController: // Controller
			return new ArrayEnumBehavior(common, ws->controller, common.texts.controllers);
		
		case PlSaveProfile: // Save profile
			return new ProfileSaveBehavior(common, *ws, false);
			
		case PlSaveProfileAs: // Save profile as
			return new ProfileSaveBehavior(common, *ws, true);
			
		case PlLoadProfile:
			return new ProfileLoadBehavior(common, *ws);

		case PlLoadedProfile:
			return new ProfileLoadedBehavior(common, *ws);
			
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
		
		int selection = menuLoop();
		
		if(selection == MainMenu::MaNewGame)
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
		else if(selection == MainMenu::MaResumeGame)
		{
			
		}
		else if(selection == MainMenu::MaQuit) // QUIT TO OS
		{
			break;
		}
		else if(selection == MainMenu::MaReplay)
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
	}

	controller.reset();
}

void Gfx::saveSettings(std::string const& path)
{
	settingsFile = path + ".dat";
	settings->save(settingsFile, rand);
}

bool Gfx::loadSettings(std::string const& path)
{
	settingsFile = path;
	settings.reset(new Settings);
	return settings->load(settingsFile, rand);
}

void Gfx::drawBasicMenu(/*int curSel*/)
{
	std::memcpy(screenBmp.pixels, &frozenScreen[0], frozenScreen.size());

	mainMenu.draw(*common, curMenu != &mainMenu, -1, true);
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
	Common& common = *this->common;
	std::memset(pal.entries, 0, sizeof(pal.entries));
	flip();
	process();
	
	fillRect(screenBmp, 0, 151, 160, 7, 0);
	common.font.drawText(screenBmp, LS(Copyright2), 2, 152, 19);

	int startItemId;
	if (controller->running())
	{
		mainMenu.setVisibility(MainMenu::MaResumeGame, true);
		mainMenu.itemFromId(MainMenu::MaResumeGame)->string = "RESUME GAME (F1)";
		mainMenu.itemFromId(MainMenu::MaNewGame)->string = "NEW GAME";
		startItemId = MainMenu::MaResumeGame;
	}
	else
	{
		mainMenu.setVisibility(MainMenu::MaResumeGame, false);
		mainMenu.itemFromId(MainMenu::MaNewGame)->string = "NEW GAME (F1)";
		startItemId = MainMenu::MaNewGame;
	}
	
	mainMenu.moveToFirstVisible();
	settingsMenu.moveToFirstVisible();
	settingsMenu.updateItems(common);
	
	fadeValue = 0;
	curMenu = &mainMenu;

	std::memcpy(&frozenScreen[0], screenBmp.pixels, frozenScreen.size());

	menuCycles = 0;
	int selected = -1;
		
	do
	{
		drawBasicMenu();
		
		if(curMenu == &mainMenu)
			settingsMenu.draw(common, true);
		else
			curMenu->draw(common, false);
		
		if(testSDLKeyOnce(SDLK_ESCAPE))
		{
			if(curMenu == &mainMenu)
				mainMenu.moveToId(MainMenu::MaQuit);
			else
				curMenu = &mainMenu;
		}
		
		if(testSDLKeyOnce(SDLK_UP))
		{
			sfx.play(common, 26);
			curMenu->movement(-1);
		}
		
		if(testSDLKeyOnce(SDLK_DOWN))
		{
			sfx.play(common, 25);
			curMenu->movement(1);
		}

		if(testSDLKeyOnce(SDLK_RETURN)
		|| testSDLKeyOnce(SDLK_KP_ENTER))
		{
			if(curMenu == &mainMenu)
			{
				sfx.play(common, 27);
				
				int s = mainMenu.selectedId();
				switch (s)
				{
					case MainMenu::MaSettings:
					{
						curMenu = &settingsMenu; // Go into settings menu
						break;
					}

					case MainMenu::MaPlayer1Settings:
					case MainMenu::MaPlayer2Settings:
					{
						playerSettings(s - MainMenu::MaPlayer1Settings);
						break;
					}

					case MainMenu::MaAdvanced:
					{
						openHiddenMenu();
						break;
					}

					case MainMenu::MaReplays:
					{
						selected = curMenu->onEnter(common);
						break;
					}

					default:
					{
						curMenu = &mainMenu;
						selected = s;
					}
				}
			}
			else if(curMenu == &settingsMenu)
			{
				settingsMenu.onEnter(common);
			}
			else
			{
				selected = curMenu->onEnter(common);
			}
		}
		
		if(testSDLKeyOnce(SDLK_F1))
		{
			curMenu = &mainMenu;
			mainMenu.moveToId(startItemId);
			selected = startItemId;
		}
		if(testSDLKeyOnce(SDLK_F2))
		{
			mainMenu.moveToId(MainMenu::MaAdvanced);
			openHiddenMenu();
		}
		if(testSDLKeyOnce(SDLK_F3))
		{
			curMenu = &mainMenu;
			mainMenu.moveToId(MainMenu::MaReplays);
			selected = curMenu->onEnter(common);
		}
		if(testSDLKeyOnce(SDLK_F10))
		{
			curMenu = &mainMenu;
			selected = MainMenu::MaQuit;
		}

		if (testSDLKeyOnce(SDLK_1))
		{
			mainMenu.moveToId(MainMenu::MaPlayer1Settings);
			playerSettings(0);
		}
		if (testSDLKeyOnce(SDLK_2))
		{
			mainMenu.moveToId(MainMenu::MaPlayer2Settings);
			playerSettings(1);
		}
		if (testSDLKeyOnce(SDLK_3))
		{
			mainMenu.moveToId(MainMenu::MaSettings);
			curMenu = &settingsMenu; // Go into settings menu
		}

		if(testSDLKey(SDLK_LEFT))
		{
			if(!curMenu->onLeftRight(common, -1))
				resetLeftRight();
		}
		if(testSDLKey(SDLK_RIGHT))
		{
			if(!curMenu->onLeftRight(common, 1))
				resetLeftRight();
		}
		
		if(testSDLKeyOnce(SDLK_PAGEUP))
		{
			sfx.play(common, 26);
				
			curMenu->movementPage(-1);
		}
			
		if(testSDLKeyOnce(SDLK_PAGEDOWN))
		{
			sfx.play(common, 25);
				
			curMenu->movementPage(1);
		}

		menuFlip();
		process();
	}
	while(selected < 0);

	for(fadeValue = 32; fadeValue > 0; --fadeValue)
	{
		menuFlip(true);
		process();
	}
	
	return selected;
}


