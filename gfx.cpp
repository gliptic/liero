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
#include <SDL2/SDL.h>
#include <cstdio>
#include <memory>
#include <limits>

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
				//ws.saveProfile(joinPath(joinPath(configRoot, "Profiles"), name));
				ws.saveProfile(gfx.getConfigNode() / "Profiles" / (name + ".lpf"));
			}
				
			sfx.play(common, 27);
		}
		else
			ws.saveProfile(ws.profileNode);
		
		menu.updateItems(common);
		return -1;
	}
	
	void onUpdate(Menu& menu, MenuItem& item)
	{
		if(!saveAs)
		{
			item.visible = (bool)ws.profileNode;
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
		if (ws.profileNode)
		{
			item.value = getBasename(getLeaf(ws.profileNode.fullPath()));
			item.visible = true;
		}
		else
		{
			item.value.clear();
			item.visible = false;
		}

		item.hasValue = true;
	}
	
	Common& common;
	WormSettings& ws;
};

#define MIN3(a, b, c) ((a) < (b) ? ((a) < (c) ? (a) : (c)) : ((b) < (c) ? (b) : (c)))
 
int levenshtein(char const *s1, char const *s2) {
    std::size_t x, y, s1len, s2len;
    s1len = strlen(s1);
    s2len = strlen(s2);
	std::size_t w = s1len+1;
	std::vector<unsigned> matrix(w * (s2len + 1));
    matrix[0] = 0;
    for (x = 1; x <= s2len; x++)
        matrix[x*w] = matrix[(x-1)*w] + 1;
    for (y = 1; y <= s1len; y++)
        matrix[y] = matrix[y-1] + 1;
    for (x = 1; x <= s2len; x++)
        for (y = 1; y <= s1len; y++)
		{
			int c = std::tolower(s1[y-1]) == std::tolower(s2[x-1]) ? 0 : 1;
            matrix[x*w + y] = MIN3(matrix[(x-1)*w + y] + 1, matrix[x*w + y - 1] + 1, matrix[(x-1)*w + y - 1] + c);
		}
 
    return(matrix[s2len*w + s1len]);
}

struct WeaponEnumBehavior : EnumBehavior
{
	WeaponEnumBehavior(Common& common, uint32_t& v)
	: EnumBehavior(common, v, 1, (uint32_t)common.weapons.size(), false)
	{
	}
		
	void onUpdate(Menu& menu, MenuItem& item)
	{
		item.value = common.weapons[common.weapOrder[v - 1]].name;
		item.hasValue = true;
	}

	int onEnter(Menu& menu, MenuItem& item)
	{
		sfx.play(common, 27);

		int x, y;
		if(!menu.itemPosition(item, x, y))
			return -1;
			
		x += menu.valueOffsetX + 2;

		std::string search;
		if (gfx.inputString(search, 10, x, y))
		{
			uint32_t minimumi;
			double minimum = std::numeric_limits<double>::max();
			for (uint32_t i = min; i <= max; ++i)
			{
				std::string& name = common.weapons[common.weapOrder[i - 1]].name;

				double dist = levenshtein(name.c_str(), search.c_str()) / (double)name.length();
				if (dist < minimum)
				{
					minimumi = i;
					minimum = dist;
				}
			}

			v = minimumi;
			menu.updateItems(common);
		}

		return -1;
	}
};

Gfx::Gfx()
: mainMenu(53, 20)
, settingsMenu(178, 20)
, playerMenu(178, 20)
, hiddenMenu(178, 20)
, curMenu(0)
, back(0)
, running(true)
, fullscreen(false)
, doubleRes(true)
, menuCycles(0)
, windowW(320 * 2)
, windowH(200 * 2)
, prevMag(0)
, keyBufPtr(keyBuf)
{
	clearKeys();
}

void Gfx::init()
{
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
	int flags = SDL_WINDOW_RESIZABLE;

	if (fullscreen)
	{
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	}

	if (window) {
		SDL_DestroyWindow(window);
	}
	window = SDL_CreateWindow("Liero 1.37", SDL_WINDOWPOS_UNDEFINED, 
	                          SDL_WINDOWPOS_UNDEFINED, windowW, windowH, flags);
	if (renderer) {
		SDL_DestroyRenderer(renderer);
	}
	// vertical sync is always enabled, because without it Liero will always
	// run at the maximum speed your computer can manage. On my machine, this
	// means it will draw so fast you can't even see the results. Of course,
	// the proper way to fix this is to decouple the drawing from the game
	// logic, but that's a pretty big undertaking. Any modern (or even old) 
	// machine should be able to run Liero with vsync without problems.
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);

	onWindowResize();
}

void Gfx::onWindowResize()
{
	if (texture) {
		SDL_DestroyTexture(texture);
	}
	texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, 
	                            SDL_TEXTUREACCESS_STREAMING, windowW, windowH);
	if (back) {
		SDL_FreeSurface(back);
	}
	back = SDL_CreateRGBSurface(0, windowW, windowH, 32, 0, 0, 0, 0);
	// linear for that old-school chunky look, but consider adding a user 
	// option for this
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
	// FIXME: we should use SDL's logical size functionality instead of the
	// manual rescaling we do now
	SDL_RenderSetLogicalSize(renderer, windowW, windowH);
	doubleRes = (windowW >= 640 && windowH >= 400);
}

void Gfx::loadMenus()
{
	hiddenMenu.addItem(MenuItem(48, 7, "FULLSCREEN (F11)", HiddenMenu::Fullscreen));
	hiddenMenu.addItem(MenuItem(48, 7, "DOUBLE SIZE", HiddenMenu::DoubleRes));
	hiddenMenu.addItem(MenuItem(48, 7, "POWERLEVEL PALETTES", HiddenMenu::LoadPowerLevels));
	hiddenMenu.addItem(MenuItem(48, 7, "SHADOWS", HiddenMenu::Shadows));
	hiddenMenu.addItem(MenuItem(48, 7, "AUTO-RECORD REPLAYS", HiddenMenu::RecordReplays));
	hiddenMenu.addItem(MenuItem(48, 7, "AI FRAMES", HiddenMenu::AiFrames));
	hiddenMenu.addItem(MenuItem(48, 7, "AI MUTATIONS", HiddenMenu::AiMutations));
	hiddenMenu.addItem(MenuItem(48, 7, "AI PARALLELS", HiddenMenu::AiParallels));
	hiddenMenu.addItem(MenuItem(48, 7, "AI TRACES", HiddenMenu::AiTraces));
	hiddenMenu.addItem(MenuItem(48, 7, "PALETTE", HiddenMenu::PaletteSelect));
	hiddenMenu.addItem(MenuItem(48, 7, "BOT WEAPONS", HiddenMenu::SelectBotWeapons));
	hiddenMenu.addItem(MenuItem(48, 7, "SEE SPAWN POINT", HiddenMenu::AllowViewingSpawnPoint));
	hiddenMenu.addItem(MenuItem(48, 7, "SINGLE SCREEN REPLAY", HiddenMenu::SingleScreenReplay));

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
	mainMenu.addItem(MenuItem(48, 48, "TC", MainMenu::MaTc));
	mainMenu.addItem(MenuItem(6, 6, "QUIT TO OS", MainMenu::MaQuit));
	mainMenu.addItem(MenuItem::space());
	mainMenu.addItem(MenuItem(48, 48, "LEFT PLAYER (F5)", MainMenu::MaPlayer1Settings));
	mainMenu.addItem(MenuItem(48, 48, "RIGHT PLAYER (F6)", MainMenu::MaPlayer2Settings));
	mainMenu.addItem(MenuItem(48, 48, "MATCH SETUP (F7)", MainMenu::MaSettings));

	settingsMenu.valueOffsetX = 100;
	playerMenu.valueOffsetX = 95;
	hiddenMenu.valueOffsetX = 120;
}

void Gfx::setFullscreen(bool newFullscreen)
{
	if (newFullscreen == fullscreen)
		return;
	fullscreen = newFullscreen;

	// fullscreen will automatically set window size
	if (!fullscreen)
	{
		if (doubleRes)
		{
			windowW = 640;
			windowH = 400;
		}
		else
		{
			windowW = 320;
			windowH = 200;
		}
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

			SDL_Scancode s = ev.key.keysym.scancode;

			if (keyBufPtr < keyBuf + 32)
				*keyBufPtr++ = ev.key.keysym;

			Uint32 dosScan = SDLToDOSKey(ev.key.keysym.scancode);
			if(dosScan)
			{
				dosKeys[dosScan] = true;
				if(controller)
					controller->onKey(dosScan, true);
			}

			if(s == SDL_SCANCODE_F11)
			{
				setFullscreen(!fullscreen);
			}
		}
		break;

		case SDL_KEYUP:
		{
			SDL_Scancode s = ev.key.keysym.scancode;

			Uint32 dosScan = SDLToDOSKey(s);
			if(dosScan)
			{
				dosKeys[dosScan] = false;
				if(controller)
					controller->onKey(dosScan, false);
			}
		}
		break;

		case SDL_WINDOWEVENT:
		{
			switch (ev.window.event)
			{
				case SDL_WINDOWEVENT_RESIZED:
				{
					windowW = ev.window.data1;
					windowH = ev.window.data2;
					onWindowResize();
				}
				break;
				
				default:
				break;
			}
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

SDL_Keysym Gfx::waitForKey()
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

	return SDL_Keysym(); // Dummy
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
		int mag = fitScreen(back->w, back->h, renderResX, renderResY, offsetX, offsetY);
		
		gvl::rect newRect(offsetX, offsetY, renderResX * mag, renderResY * mag);
		
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
		
		uint32_t pal32[256];
		preparePalette(back->format, realPal, pal32);
		scaleDraw(src, renderResX, renderResY, srcPitch, dest, destPitch, mag, pal32);
	}

	SDL_UpdateTexture(texture, NULL, back->pixels, windowW * 4);
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);

	lastUpdateRect = updateRect;
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
	gfx.releaseSDLKey(SDL_SCANCODE_LEFT);
	gfx.releaseSDLKey(SDL_SCANCODE_RIGHT);
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
	: common(common)
	, player(player)
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
		
		std::string name = getBasename(getLeaf(gfx.settingsNode.fullPath()));
		if(gfx.inputString(name, 30, x, y) && !name.empty())
		{
			//gfx.saveSettings(joinPath(configRoot, name + ".cfg"));
			gfx.saveSettings(gfx.getConfigNode() / (name + ".cfg"));
		}
				
		sfx.play(common, 27);
		
		onUpdate(menu, item);
		return -1;
	}
	
	void onUpdate(Menu& menu, MenuItem& item)
	{
		item.value = getBasename(getLeaf(gfx.settingsNode.fullPath()));
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
		case Settings::GMScalesOfJustice:
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

using std::string;
using std::vector;
using std::pair;
using gvl::shared_ptr;

void Gfx::selectLevel()
{
	Common& common = *this->common;
	FileSelector levSel(common);

	shared_ptr<FileNode> random(new FileNode(
		LS(Random), "", "", false, &levSel.rootNode));

	{
		levSel.fill(getConfigNode(), [](string const& name, string const& ext) { return ciCompare(ext, "LEV"); });

		random->id = 1;
		levSel.rootNode.children.insert(levSel.rootNode.children.begin(), random);
		levSel.setFolder(levSel.rootNode);
		levSel.select(settings->levelFile);
	}

	FileNode* previewNode = 0;

	do
	{
		screenBmp.copy(frozenScreen);
		
		string title = LS(SelLevel);
		if (!levSel.currentNode->fullPath.empty())
		{
			title += ' ';
			title += levSel.currentNode->fullPath;
		}
		
		int wid = common.font.getDims(title);

		drawRoundedBox(screenBmp, 178, 20, 0, 7, wid);
		common.font.drawText(screenBmp, title, 180, 21, 50);

		FileNode* sel = levSel.curSel();
		if (previewNode != sel && sel && sel != random.get() && !sel->folder)
		{
			Level level(common);

			ReaderFile f;

			try
			{
				if (level.load(common, *settings, sel->getFsNode().toOctetReader()))
				{
					level.drawMiniature(frozenScreen, 134, 162, 10);
				}
			}
			catch (std::runtime_error&)
			{
				// Ignore
			}

			previewNode = sel;
		}

		levSel.draw();
		
		if (!levSel.process())
			break;

		if(testSDLKeyOnce(SDL_SCANCODE_RETURN)
		|| testSDLKeyOnce(SDL_SCANCODE_KP_ENTER))
		{
			sfx.play(common, 27);
			
			auto* sel = levSel.enter();

			if (sel)
			{
				if (sel == random.get())
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
		profileSel.fill(getConfigNode(), [](string const& name, string const& ext) { return ciCompare(ext, "LPF"); });

		profileSel.setFolder(profileSel.rootNode);
		profileSel.select(joinPath(getConfigNode().fullPath(), "Profiles"));
	}
	
	do
	{
		screenBmp.copy(frozenScreen);

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

		if(testSDLKeyOnce(SDL_SCANCODE_RETURN)
		|| testSDLKeyOnce(SDL_SCANCODE_KP_ENTER))
		{
			auto* sel = profileSel.enter();

			if (sel)
			{
				ws.loadProfile(sel->getFsNode());
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
		replaySel.fill(getConfigNode(), [](string const& name, string const& ext) { return ciCompare(ext, "LRP"); });

		replaySel.setFolder(replaySel.rootNode);
		if (prevSelectedReplayPath.empty()
		  || !replaySel.select(prevSelectedReplayPath))
		{
			replaySel.select(joinPath(getConfigNode().fullPath(), "Replays"));
		}
	}

	do
	{
		screenBmp.copy(frozenScreen);

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

		if(testSDLKeyOnce(SDL_SCANCODE_RETURN)
		|| testSDLKeyOnce(SDL_SCANCODE_KP_ENTER))
		{
			auto* sel = replaySel.enter();

			if (sel)
			{
				prevSelectedReplayPath = sel->fullPath;

				// Reset controller before opening the replay, since we may be recording it
				controller.reset();

				controller.reset(new ReplayController(common, sel->getFsNode().toSource()));
				
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
		optionsSel.fill(getConfigNode(), [](string const& name, string const& ext) {
			return ciCompare(ext, "CFG");
		});

		optionsSel.setFolder(optionsSel.rootNode);
	}
	
	do
	{
		screenBmp.copy(frozenScreen);
		
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

		if(testSDLKeyOnce(SDL_SCANCODE_RETURN)
		|| testSDLKeyOnce(SDL_SCANCODE_KP_ENTER))
		{
			auto* sel = optionsSel.enter();

			if (sel)
			{
				gfx.loadSettings(sel->getFsNode());
				return;
			}
		}
		menuFlip();
		process();
	}
	while(true);
}

std::unique_ptr<Common> Gfx::selectTc()
{
	FileSelector tcSel(*common, 28);

	{
		tcSel.fill(getConfigNode() / "TC", 0);

		tcSel.setFolder(tcSel.rootNode);

		auto end = std::remove_if(tcSel.rootNode.children.begin(), tcSel.rootNode.children.end(), [](shared_ptr<FileNode> const& n) {
			auto tc = n->getFsNode() / "tc.cfg";
			return !tc.exists();
		});

		tcSel.rootNode.children.erase(end, tcSel.rootNode.children.end());

		for (auto& c : tcSel.rootNode.children)
		{
			c->folder = false;
		}
	}
	
	do
	{
		screenBmp.copy(frozenScreen);
		
		string title = "Select TC:";
		if (!tcSel.currentNode->fullPath.empty())
		{
			title += ' ';
			title += tcSel.currentNode->fullPath;
		}
		
		common->font.drawFramedText(screenBmp, title, 178, 20, 50);

		tcSel.draw();
		
		if (!tcSel.process())
			break;

		if(testSDLKeyOnce(SDL_SCANCODE_RETURN)
		|| testSDLKeyOnce(SDL_SCANCODE_KP_ENTER))
		{
			auto* sel = tcSel.enter();

			if (sel)
			{
				gvl::unique_ptr<Common> common(new Common());
				common->load(sel->getFsNode());
				return std::move(common);
			}
		}
		menuFlip();
		process();
	}
	while(true);
	
	return std::unique_ptr<Common>();
}

struct WeaponMenu : Menu
{
	WeaponMenu(int x, int y)
	: Menu(x, y)
	{
	}
	
	ItemBehavior* getItemBehavior(Common& common, MenuItem& item)
	{
		int index = common.weapOrder[item.id];
		return new ArrayEnumBehavior(common, gfx.settings->weapTable[index], common.texts.weapStates);
	}
};

void Gfx::weaponOptions()
{
	Common& common = *this->common;
	WeaponMenu weaponMenu(179, 28);
			
	weaponMenu.setHeight(14);
	weaponMenu.valueOffsetX = 89;
	
	for(int i = 0; i < (int)common.weapons.size(); ++i)
	{
		int index = common.weapOrder[i];
		weaponMenu.addItem(MenuItem(48, 7, common.weapons[index].name, i));
	}
	
	weaponMenu.moveToFirstVisible();
	weaponMenu.updateItems(common);
	
	while(true)
	{
		screenBmp.copy(frozenScreen);
		
		drawBasicMenu();
		
		drawRoundedBox(screenBmp, 179, 20, 0, 7, common.font.getDims(LS(Weapon)));
		drawRoundedBox(screenBmp, 249, 20, 0, 7, common.font.getDims(LS(Availability)));
		
		common.font.drawText(screenBmp, LS(Weapon), 181, 21, 50);
		common.font.drawText(screenBmp, LS(Availability), 251, 21, 50);
		
		weaponMenu.draw(common, false);

		if(testSDLKeyOnce(SDL_SCANCODE_UP))
		{
			sfx.play(common, 26);
			weaponMenu.movement(-1);
		}

		if(testSDLKeyOnce(SDL_SCANCODE_DOWN))
		{
			sfx.play(common, 25);
			weaponMenu.movement(1);
		}

		if(testSDLKeyOnce(SDL_SCANCODE_LEFT))
		{
			weaponMenu.onLeftRight(common, -1);
		}
		if(testSDLKeyOnce(SDL_SCANCODE_RIGHT))
		{
			weaponMenu.onLeftRight(common, 1);
		}
		
		if(settings->extensions)
		{
			if(testSDLKeyOnce(SDL_SCANCODE_PAGEUP))
			{
				sfx.play(common, 26);
				
				weaponMenu.movementPage(-1);
			}

			if(testSDLKeyOnce(SDL_SCANCODE_PAGEDOWN))
			{
				sfx.play(common, 25);
				
				weaponMenu.movementPage(1);
			}
		}

		weaponMenu.onKeys(gfx.keyBuf, gfx.keyBufPtr);

		menuFlip();
		process();

		if(testSDLKeyOnce(SDL_SCANCODE_ESCAPE))
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
		
		SDL_Event ev;

		//int offset = clrX + y*320; // TODO: Unhardcode 320
		
		blitImageNoKeyColour(screenBmp, &frozenScreen.getPixel(clrX, y), clrX, y, clrX + 10 + width, 8, frozenScreen.pitch);
		
		drawRoundedBox(screenBmp, x - 2 - adjust, y, 0, 7, width);
		
		font.drawText(screenBmp, str, x - adjust, y + 1, 50);
		flip();

		SDL_StartTextInput();
		SDL_WaitEvent(&ev);
		processEvent(ev);

		switch (ev.type)
		{
			case SDL_KEYDOWN:
				switch (ev.key.keysym.scancode)
				{
					case SDL_SCANCODE_BACKSPACE:
						if(!buffer.empty())
						{
							buffer.erase(buffer.size() - 1);
						}
					break;

					case SDL_SCANCODE_RETURN:
					case SDL_SCANCODE_KP_ENTER:
						dest = buffer;
						sfx.play(*common, 27);
						clearKeys();
						return true;

					case SDL_SCANCODE_ESCAPE:
						clearKeys();
						return false;
					break;
				}
				break;

			case SDL_TEXTINPUT:
			{
				int k = utf8ToDOS(ev.text.text);
				if (k && buffer.size() < maxLen &&
					(!filter || (k = filter(k))))
				{
					buffer += char(k);
				}
				break;
			}
            case SDL_TEXTEDITING:
				// since there's no support for any characters that can use a
            	// complex IME input (like East Asian languages), we naively
            	// discard this event
                break;
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
restart:
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
			std::unique_ptr<Controller> newController(new LocalController(common, settings));
			
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
			
			controller = std::move(newController);
		}
		else if(selection == MainMenu::MaResumeGame)
		{
			if (controller->isReplay() && settings->singleScreenReplay)
			{
				renderResX = 640;
				renderResY = 400;
			}			
		}
		else if(selection == MainMenu::MaQuit) // QUIT TO OS
		{
			break;
		}
		else if(selection == MainMenu::MaReplay)
		{
			if (settings->singleScreenReplay)
			{
				renderResX = 640;
				renderResY = 400;
			}
		}
		else if (selection == MainMenu::MaTc)
		{
			goto restart;
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

		// reset internal resolution upon exiting any game. This includes
		// replays, because the menu needs to render at the same resolution
		renderResX = 320;
		renderResY = 200;	
		
		controller->unfocus();
		
		clearKeys();
	}

	controller.reset();
}

void Gfx::saveSettings(FsNode node)
{
	settingsNode = node;
	settings->save(node, rand);
}

bool Gfx::loadSettings(FsNode node)
{
	settingsNode = node;
	settings.reset(new Settings);
	return settings->load(node, rand);
}

bool Gfx::loadSettingsLegacy(FsNode node)
{
	settings.reset(new Settings);
	return settings->loadLegacy(node, rand);
}

void Gfx::drawBasicMenu(/*int curSel*/)
{
	screenBmp.copy(frozenScreen);

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

	frozenScreen.copy(screenBmp);

	menuCycles = 0;
	int selected = -1;
		
	do
	{
		drawBasicMenu();
		
		if(curMenu == &mainMenu)
			settingsMenu.draw(common, true);
		else
			curMenu->draw(common, false);

		if(testSDLKeyOnce(SDL_SCANCODE_ESCAPE))
		{
			if(curMenu == &mainMenu)
				mainMenu.moveToId(MainMenu::MaQuit);
			else
				curMenu = &mainMenu;
		}

		if(testSDLKeyOnce(SDL_SCANCODE_UP))
		{
			sfx.play(common, 26);
			curMenu->movement(-1);
		}

		if(testSDLKeyOnce(SDL_SCANCODE_DOWN))
		{
			sfx.play(common, 25);
			curMenu->movement(1);
		}

		if(testSDLKeyOnce(SDL_SCANCODE_RETURN)
		|| testSDLKeyOnce(SDL_SCANCODE_KP_ENTER))
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

					case MainMenu::MaTc:
					{
						if (curMenu->onEnter(common) == MainMenu::MaTc)
							return MainMenu::MaTc;
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

		if(testSDLKeyOnce(SDL_SCANCODE_F1))
		{
			curMenu = &mainMenu;
			mainMenu.moveToId(startItemId);
			selected = startItemId;
		}
		if(testSDLKeyOnce(SDL_SCANCODE_F2))
		{
			mainMenu.moveToId(MainMenu::MaAdvanced);
			openHiddenMenu();
		}
		if(testSDLKeyOnce(SDL_SCANCODE_F3))
		{
			curMenu = &mainMenu;
			mainMenu.moveToId(MainMenu::MaReplays);
			selected = curMenu->onEnter(common);
		}

		if (testSDLKeyOnce(SDL_SCANCODE_F5))
		{
			mainMenu.moveToId(MainMenu::MaPlayer1Settings);
			playerSettings(0);
		}
		if (testSDLKeyOnce(SDL_SCANCODE_F6))
		{
			mainMenu.moveToId(MainMenu::MaPlayer2Settings);
			playerSettings(1);
		}
		if (testSDLKeyOnce(SDL_SCANCODE_F7))
		{
			mainMenu.moveToId(MainMenu::MaSettings);
			curMenu = &settingsMenu; // Go into settings menu
		}

#if 1
		if (testSDLKeyOnce(SDL_SCANCODE_F8))
		{
			uint32 s = 14;
			
			Rand r;
			r.seed(s);

			Common& common = *this->common;

			vector<std::size_t> nobjMap;

			for (std::size_t i = 0; i < common.nobjectTypes.size(); ++i)
			{
				nobjMap.push_back(i);
			}

			std::random_shuffle(nobjMap.begin(), nobjMap.end(), r);

			for (auto& w : common.weapons)
			{
				w.addSpeed = r(30) - 5;
				w.affectByExplosions = r(2) == 0;
				w.affectByWorm = r(3) == 0;
				w.ammo = r(20) + 1;
				w.bloodOnHit = r(50);
				w.blowAway = r(10);
				w.bounce = r(90);
				w.collideWithObjects = r(10) == 0;
				w.colorBullets = 3 + r(250);
				w.createOnExp = r(common.sobjectTypes.size());
				w.delay = r(70);
				w.detectDistance = r(20);
				w.dirtEffect = r(9);
				w.distribution = r(5000) - 2500;
				w.explGround = r(2) == 0;
				w.exploSound = r(common.sounds.size());
				w.fireCone = r(10);
				w.gravity = r(2000) - 1000;
				w.hitDamage = r(20);
				w.laserSight = r(5) == 0;
				w.launchSound = r(common.sounds.size());
				w.leaveShellDelay = r(30);
				w.leaveShells = r(1) == 0;
				w.loadingTime = r(70 * 3);
				w.loopAnim = r(10) == 0;
				w.loopSound = false;
				w.multSpeed = r(2) ? 100 : 99 + r(5);
				w.objTrailDelay = 10 + r(70);
				w.objTrailType = r(4) == 0 ? r(common.sobjectTypes.size()) : -1;
				w.parts = r(2) == 0 ? r(10) : 1;
				w.partTrailDelay = 10 + r(70);
				w.partTrailObj = r(4) == 0 ? r(common.nobjectTypes.size()) : -1;
				w.partTrailType = r(2);
				w.playReloadSound = r(2) == 0;
				w.recoil = r(20);
				w.shadow = r(2) == 0;
				w.shotType = r(5);
				w.speed = r(200);
				w.splinterAmount = r(5) == 0 ? r(10) : 0;
				w.splinterColour = r(256);
				w.splinterScatter = r(2);
				w.splinterType = r(common.nobjectTypes.size());
				w.startFrame = r((uint32)common.smallSprites.count - 13);
				w.numFrames = r(5);
				w.timeToExplo = 50 + r(200);
				w.timeToExploV = 10 + r(50);
				w.wormCollide = r(3) > 0;
				w.wormExplode = r(3) > 0;
			}

			//for (auto& n : common.nobjectTypes)
			for (std::size_t idx = 0; idx < common.nobjectTypes.size(); ++idx)
			{
				auto& n = common.nobjectTypes[nobjMap[idx]];
				n.affectByExplosions = r(5) == 0;
				n.bloodOnHit = r(5);
				n.bloodTrail = r(10) == 0;
				n.bloodTrailDelay = r(20) + 3;
				n.blowAway = r(10);
				n.bounce = r(90);
				n.colorBullets = 3 + r(250);
				n.createOnExp = r(3) == 0 ? r(common.sobjectTypes.size()) : -1;
				n.detectDistance = r(20);
				n.dirtEffect = r(9);
				n.distribution = r(5000) - 2500;
				n.drawOnMap = r(20) == 0;
				n.explGround = r(4) > 0;
				n.gravity = r(2000) - 1000;
				n.hitDamage = r(10);
				n.leaveObj = r(5) == 0 ? r(common.sobjectTypes.size()) : -1;
				n.leaveObjDelay = 10 + r(80);
				n.startFrame = r((uint32)common.smallSprites.count - 13);
				n.numFrames = r(5);
				n.speed = r(150);
				n.splinterAmount = idx > 0 && r(5) == 0 ? r(10) : 0;
				n.splinterColour = r(256);
				n.splinterType = idx > 0 ? nobjMap[r(idx)] : 0;
				n.timeToExplo = 50 + r(70 * 3);
				n.timeToExploV = r(30);
				n.wormDestroy = r(3) == 0;
				n.wormExplode = r(2) == 0;
			}

			for (auto& s : common.sobjectTypes)
			{
				s.animDelay = 1 + r(10);
				s.blowAway = r(2) == 0 ? r(10000) : 0;
				s.damage = r(30);
				s.detectRange = r(20);
				s.dirtEffect = r(9);
				s.flash = r(5);
				s.startFrame = r((uint32)common.largeSprites.count - 7);
				s.numFrames = r(7);
				s.startSound = r(common.sounds.size());
				s.shake = r(10);
				s.shadow = r(2);
				s.numSounds = 1;
			}
		}
#endif

		if(testSDLKey(SDL_SCANCODE_LEFT))
		{
			if(!curMenu->onLeftRight(common, -1))
				resetLeftRight();
		}
		if(testSDLKey(SDL_SCANCODE_RIGHT))
		{
			if(!curMenu->onLeftRight(common, 1))
				resetLeftRight();
		}

		if(testSDLKeyOnce(SDL_SCANCODE_PAGEUP))
		{
			sfx.play(common, 26);
				
			curMenu->movementPage(-1);
		}

		if(testSDLKeyOnce(SDL_SCANCODE_PAGEDOWN))
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


