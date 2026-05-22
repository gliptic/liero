#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <cstring>
#include <cassert>
#include <cstdlib>
#include <cctype>
#include <vector>
#include <unordered_map>
#include <utility>
#include <algorithm>
#include <cstdio>
#include <memory>
#include <random>

#if OPENLIERO_EMSCRIPTEN
#include <emscripten.h>
#endif

#include "gfx.hpp"
#include "reader.hpp"
#include "game.hpp"
#include "sfx.hpp"
#include "text.hpp"
#include "keys.hpp"
#include "filesystem.hpp"
#include "metadata.hpp"

#include <gvl/io2/fstream.hpp>

#include "controller/replayController.hpp"
#include "controller/localController.hpp"
#include "controller/controller.hpp"

#include "mainMenuState.hpp"
#include "gamePlayState.hpp"
#include "netConnectState.hpp"
#include "onlineConnectState.hpp"

#include "gfx/macros.hpp"

#include "menu/arrayEnumBehavior.hpp"

Gfx gfx;

struct KeyBehavior : ItemBehavior
{
	KeyBehavior(Common& common, uint32_t& key, uint32_t& keyEx, uint32_t& gamepadKey, uint32_t& inputDevice, bool extended = false)
	: common(common)
	, key(key)
	, keyEx(keyEx)
	, gamepadKey(gamepadKey)
	, inputDevice(inputDevice)
	, extended(extended)
	{
	}

	void onUpdate(Menu& menu, MenuItem& item)
	{
		if (inputDevice != WormSettingsExtensions::InputKeyboard)
			item.value = gfx.getGamepadKeyName(gamepadKey);
		else
			item.value = gfx.getKeyName(extended ? keyEx : key);
		item.hasValue = true;
	}

	Common& common;
	uint32_t& key;
	uint32_t& keyEx;
	uint32_t& gamepadKey;
	uint32_t& inputDevice;
	bool extended;
};

struct WormNameBehavior : ItemBehavior
{
	WormNameBehavior(Common& common, WormSettings& ws)
	: common(common)
	, ws(ws)
	{
	}

	void onUpdate(Menu& menu, MenuItem& item)
	{
		item.value = ws.name;
		item.hasValue = true;
	}

	Common& common;
	WormSettings& ws;
};

struct InputDeviceBehavior : ItemBehavior
{
	struct GamepadOption {
		std::string name;
		std::string serial;
		std::string displayName;
		int joystickIdx;
	};

	InputDeviceBehavior(Common& common, WormSettings& ws)
	: common(common)
	, ws(ws)
	{
	}

	std::vector<GamepadOption> buildOptions()
	{
		std::vector<GamepadOption> opts;
		// Count names to detect duplicates
		std::unordered_map<std::string, int> nameCounts;
		for (int i = 0; i < (int)gfx.joysticks.size(); ++i)
		{
			char const* n = SDL_GetGamepadName(gfx.joysticks[i].sdlGamepad);
			std::string name = n ? n : "Gamepad";
			nameCounts[name]++;
		}

		std::unordered_map<std::string, int> nameSeenSoFar;
		for (int i = 0; i < (int)gfx.joysticks.size(); ++i)
		{
			GamepadOption opt;
			char const* n = SDL_GetGamepadName(gfx.joysticks[i].sdlGamepad);
			opt.name = n ? n : "Gamepad";
			char const* s = SDL_GetGamepadSerial(gfx.joysticks[i].sdlGamepad);
			opt.serial = s ? s : "";
			opt.joystickIdx = i;

			// Disambiguate display name if duplicates exist
			if (nameCounts[opt.name] > 1)
			{
				int idx = ++nameSeenSoFar[opt.name];
				std::string suffix = " #" + toString(idx);
				std::string base = opt.name.substr(0, 20 - suffix.size());
				opt.displayName = base + suffix;
			}
			else
			{
				opt.displayName = opt.name.substr(0, 20);
			}
			opts.push_back(opt);
		}
		return opts;
	}

	int findCurrentOption(std::vector<GamepadOption> const& opts)
	{
		if (ws.inputDevice == WormSettingsExtensions::InputKeyboard)
			return -1;
		// Try serial match first
		if (!ws.gamepadSerial.empty())
		{
			for (int i = 0; i < (int)opts.size(); ++i)
				if (opts[i].name == ws.gamepadName && opts[i].serial == ws.gamepadSerial)
					return i;
		}
		// Fall back to name match
		for (int i = 0; i < (int)opts.size(); ++i)
			if (opts[i].name == ws.gamepadName)
				return i;
		return -1;
	}

	void onUpdate(Menu& menu, MenuItem& item)
	{
		if (ws.inputDevice == WormSettingsExtensions::InputKeyboard)
		{
			item.value = "Keyboard";
		}
		else
		{
			auto opts = buildOptions();
			int cur = findCurrentOption(opts);
			if (cur >= 0)
				item.value = opts[cur].displayName;
			else
			{
				std::string display = ws.gamepadName.empty() ? "Gamepad (none)" : ws.gamepadName.substr(0, 20);
				item.value = display;
			}
		}
		item.hasValue = true;
	}

	bool onLeftRight(Menu& menu, MenuItem& item, int dir)
	{
		sfx.play(common, dir > 0 ? 25 : 26);
		cycle(menu, dir);
		return false;
	}

	int onEnter(Menu& menu, MenuItem& item)
	{
		sfx.play(common, 27);
		cycle(menu, 1);
		return -1;
	}

	void cycle(Menu& menu, int dir)
	{
		auto opts = buildOptions();
		// Options: -1 = keyboard, 0..N-1 = gamepads
		int count = (int)opts.size() + 1;
		int cur = findCurrentOption(opts) + 1; // shift so keyboard=0, gamepads=1..N
		cur = ((cur + dir) % count + count) % count;

		if (cur == 0)
		{
			ws.inputDevice = WormSettingsExtensions::InputKeyboard;
			ws.gamepadName.clear();
			ws.gamepadSerial.clear();
		}
		else
		{
			auto& opt = opts[cur - 1];
			ws.inputDevice = 1;
			ws.gamepadName = opt.name;
			ws.gamepadSerial = opt.serial;
		}

		menu.updateItems(common);
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

		if(!saveAs)
			ws.saveProfile(ws.profileNode);
		// saveAs path is intercepted by MainMenuState

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
};

Gfx::Gfx()
: mainMenu(53, 20)
, settingsMenu(178, 20)
, playerMenu(178, 20)
, hiddenMenu(178, 20)
, curMenu(0)
, sdlDrawSurface(0)
, running(true)
, doubleRes(true)
, menuCycles(0)
, windowW(320 * 2)
, windowH(200 * 2)
, prevMag(0)
, keyBufPtr(keyBuf)
{
	clearKeys();
	primaryRenderer = &playRenderer;
}

void Gfx::init()
{
	SDL_HideCursor();
	lastFrame = SDL_GetTicks();

	playRenderer.init(320, 200);
	singleScreenRenderer.init(640, 400);
	// Gamepad init
	SDL_SetGamepadEventsEnabled(true);
	int numGamepads = 0;
	SDL_JoystickID *gamepadIds = SDL_GetGamepads(&numGamepads);
	joysticks.resize(numGamepads);
	for ( int i = 0; i < numGamepads; ++i ) {
		joysticks[i].sdlGamepad = SDL_OpenGamepad(gamepadIds[i]);
		joysticks[i].instanceId = gamepadIds[i];
		joysticks[i].clearState();
	}
	SDL_free(gamepadIds);
}

void Gfx::setVideoMode()
{
	if (sdlSpectatorRenderer)
	{
		SDL_DestroyRenderer(sdlSpectatorRenderer);
		sdlSpectatorRenderer = NULL;
	}
	if (settings->spectatorWindow)
	{
		if (!sdlSpectatorWindow)
		{
			std::string spectatorWindowTitle = std::string("Liero Spectator Window - ") + build_version();
			sdlSpectatorWindow = SDL_CreateWindow(spectatorWindowTitle.c_str(), windowW, windowH,
				SDL_WINDOW_RESIZABLE | (spectatorFullscreen ? SDL_WINDOW_FULLSCREEN : 0));
		}
		else
		{
			SDL_SetWindowFullscreen(sdlSpectatorWindow, spectatorFullscreen);
		}
		sdlSpectatorRenderer = SDL_CreateRenderer(sdlSpectatorWindow, NULL);
		onWindowResize(SDL_GetWindowID(sdlSpectatorWindow));
	}
	else
	{
		if (sdlSpectatorTexture)
		{
			SDL_DestroyTexture(sdlSpectatorTexture);
			sdlSpectatorTexture = NULL;
		}
		if (sdlSpectatorDrawSurface)
		{
			SDL_DestroySurface(sdlSpectatorDrawSurface);
			sdlSpectatorDrawSurface = NULL;
		}
		if (sdlSpectatorWindow)
		{
			SDL_DestroyWindow(sdlSpectatorWindow);
			sdlSpectatorWindow = NULL;
		}
	}

	if (!sdlWindow)
	{
		std::string windowTitle = std::string("Liero ") + build_version();
		sdlWindow = SDL_CreateWindow(windowTitle.c_str(), windowW, windowH,
			SDL_WINDOW_RESIZABLE | (settings->fullscreen ? SDL_WINDOW_FULLSCREEN : 0));

#ifndef __APPLE__
		std::string s = (getConfigNode() / "Resources" / "icon.png").fullPath();
		SDL_Surface *icon = IMG_Load(s.c_str());
		if (icon)
		{
			SDL_SetWindowIcon(sdlWindow, icon);
			SDL_DestroySurface(icon);
		}
#endif
	}
	else
	{
		SDL_SetWindowFullscreen(sdlWindow, settings->fullscreen);
	}
	if (sdlRenderer)
	{
		SDL_DestroyRenderer(sdlRenderer);
		sdlRenderer = NULL;
	}
	// vertical sync is always disabled. Frame limiting is done manually below,
	// to keep the correct speed
	sdlRenderer = SDL_CreateRenderer(sdlWindow, NULL);
	onWindowResize(SDL_GetWindowID(sdlWindow));

	// Set the spectator window's icon after the main window has been initialized.
	// On Windows, this makes sure the icon in the stacked taskbar is the main icon.
	// On MacOS this is commented out, because it only allows one icon and the spectator icon
	// will override the main icon
#ifndef __APPLE__
	if (sdlSpectatorWindow)
	{
		std::string s = (getConfigNode() / "Resources" / "spectator_icon.png").fullPath();
		SDL_Surface *spectator_icon = IMG_Load(s.c_str());
		if (spectator_icon)
		{
			SDL_SetWindowIcon(sdlSpectatorWindow, spectator_icon);
			SDL_DestroySurface(spectator_icon);
		}
	}
#endif
}

void Gfx::onWindowResize(uint32_t windowID)
{
	if (windowID == SDL_GetWindowID(sdlWindow))
	{
		if (sdlTexture)
		{
			SDL_DestroyTexture(sdlTexture);
			sdlTexture = NULL;
		}
		sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_ARGB8888,
			                           SDL_TEXTUREACCESS_STREAMING,
			                           doubleRes ? 640 : 320,
		                         	   doubleRes ? 400 : 200);

		if (sdlDrawSurface)
		{
			SDL_DestroySurface(sdlDrawSurface);
			sdlDrawSurface = NULL;
		}
		sdlDrawSurface = SDL_CreateSurface(doubleRes ? 640 : 320,
		                         doubleRes ? 400 : 200, SDL_PIXELFORMAT_ARGB8888);
		// linear for that old-school chunky look, but consider adding a user
		// option for this
		SDL_SetTextureScaleMode(sdlTexture, SDL_SCALEMODE_LINEAR);
		SDL_SetRenderLogicalPresentation(sdlRenderer, doubleRes ? 640 : 320,
		                         doubleRes ? 400 : 200, SDL_LOGICAL_PRESENTATION_LETTERBOX);
	}
	else
	{
		if (sdlSpectatorTexture)
		{
			SDL_DestroyTexture(sdlSpectatorTexture);
			sdlSpectatorTexture = NULL;
		}
		if (sdlSpectatorDrawSurface)
		{
			SDL_DestroySurface(sdlSpectatorDrawSurface);
			sdlSpectatorDrawSurface = NULL;
		}

		if (settings->spectatorWindow)
		{
			sdlSpectatorTexture = SDL_CreateTexture(sdlSpectatorRenderer,
			                                        SDL_PIXELFORMAT_ARGB8888,
				                           			SDL_TEXTUREACCESS_STREAMING,
				                           			640, 400);
			sdlSpectatorDrawSurface = SDL_CreateSurface(640, 400, SDL_PIXELFORMAT_ARGB8888);
			SDL_SetRenderLogicalPresentation(sdlSpectatorRenderer, 640, 400, SDL_LOGICAL_PRESENTATION_LETTERBOX);
		}
	}
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
	hiddenMenu.addItem(MenuItem(48, 7, "SPECTATOR WINDOW", HiddenMenu::SpectatorWindow));

	playerMenu.addItem(MenuItem(3, 7, "PROFILE LOADED", PlayerMenu::PlLoadedProfile));
	playerMenu.addItem(MenuItem(3, 7, "SAVE PROFILE", PlayerMenu::PlSaveProfile));
	playerMenu.addItem(MenuItem(3, 7, "SAVE PROFILE AS...", PlayerMenu::PlSaveProfileAs));
	playerMenu.addItem(MenuItem(3, 7, "LOAD PROFILE", PlayerMenu::PlLoadProfile));
	playerMenu.addItem(MenuItem(48, 7, "NAME", PlayerMenu::PlName));
	playerMenu.addItem(MenuItem(48, 7, "HEALTH", PlayerMenu::PlHealth));
	playerMenu.addItem(MenuItem(48, 7, "Red", PlayerMenu::PlRed));
	playerMenu.addItem(MenuItem(48, 7, "Green", PlayerMenu::PlGreen));
	playerMenu.addItem(MenuItem(48, 7, "Blue", PlayerMenu::PlBlue));
	playerMenu.addItem(MenuItem(48, 7, "INPUT", PlayerMenu::PlInput));
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

	mainMenu.addItem(MenuItem(10, 10, "", MainMenu::MaResumeGame)); // string set in MainMenuState::enter()
	mainMenu.addItem(MenuItem(10, 10, "", MainMenu::MaNewGame)); // string set in MainMenuState::enter()
	mainMenu.addItem(MenuItem(48, 48, "HOST LAN GAME", MainMenu::MaHostGame));
	mainMenu.addItem(MenuItem(48, 48, "JOIN LAN GAME", MainMenu::MaJoinGame));
	mainMenu.addItem(MenuItem(48, 48, "HOST ONLINE", MainMenu::MaHostOnline));
	mainMenu.addItem(MenuItem(48, 48, "JOIN ONLINE", MainMenu::MaJoinOnline));
	mainMenu.addItem(MenuItem(48, 48, "OPTIONS (F2)", MainMenu::MaAdvanced));
	mainMenu.addItem(MenuItem(48, 48, "REPLAYS (F3)", MainMenu::MaReplays));
	mainMenu.addItem(MenuItem(48, 48, "TC", MainMenu::MaTc));
	mainMenu.addItem(MenuItem(6, 6, "QUIT TO OS", MainMenu::MaQuit));
	mainMenu.addItem(MenuItem::space());
	mainMenu.addItem(MenuItem(48, 48, "LEFT PLAYER (F5)", MainMenu::MaPlayer1Settings));
	mainMenu.addItem(MenuItem(48, 48, "RIGHT PLAYER (F6)", MainMenu::MaPlayer2Settings));
	mainMenu.addItem(MenuItem(48, 48, "NETWORK PLAYER (F9)", MainMenu::MaNetPlayerSettings));
	mainMenu.addItem(MenuItem(48, 48, "MATCH SETUP (F7)", MainMenu::MaSettings));

	settingsMenu.valueOffsetX = 100;
	playerMenu.valueOffsetX = 95;
	hiddenMenu.valueOffsetX = 120;
}

void Gfx::setSpectatorFullscreen(bool newFullscreen)
{
	if (newFullscreen == spectatorFullscreen)
		return;
	spectatorFullscreen = newFullscreen;

	if (!spectatorFullscreen)
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
}

void Gfx::setFullscreen(bool newFullscreen)
{
	if (newFullscreen == settings->fullscreen)
		return;
	settings->fullscreen = newFullscreen;

	// fullscreen will automatically set window size
	if (!settings->fullscreen)
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
	doubleRes = newDoubleRes;

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
		case SDL_EVENT_KEY_DOWN:
		{
			SDL_Scancode s = ev.key.scancode;

			if (keyBufPtr < keyBuf + 32)
				*keyBufPtr++ = ev.key.scancode;

			uint32_t dosScan = SDLToDOSKey(ev.key.scancode);
			if(dosScan)
			{
				dosKeys[dosScan] = true;
				if(controller && !ev.key.repeat)
					controller->onKey(dosScan, true);
			}

			if(s == SDL_SCANCODE_F11)
			{
				if (SDL_GetWindowFromID(ev.key.windowID) == sdlWindow)
				{
					setFullscreen(!settings->fullscreen);
				}
				else
				{
					setSpectatorFullscreen(!spectatorFullscreen);
				}

			}

			if(s == SDL_SCANCODE_F4 && (ev.key.mod & SDL_KMOD_ALT))
			{
				running = false;
			}
		}
		break;

		case SDL_EVENT_KEY_UP:
		{
			SDL_Scancode s = ev.key.scancode;

			uint32_t dosScan = SDLToDOSKey(s);
			if(dosScan)
			{
				dosKeys[dosScan] = false;
				if(controller)
					controller->onKey(dosScan, false);
			}
		}
		break;

		case SDL_EVENT_WINDOW_RESIZED:
		{
			onWindowResize(ev.window.windowID);
		}
		break;

		case SDL_EVENT_QUIT:
		{
			running = false;
		}
		break;

		case SDL_EVENT_GAMEPAD_AXIS_MOTION:
		{
			int gpIdx = findGamepadIndex(ev.gaxis.which);
			if (gpIdx < 0) break;
			Joystick& js = joysticks[gpIdx];
			int axis = ev.gaxis.axis;

			bool posState = (ev.gaxis.value > JoyAxisThreshold);
			bool negState = (ev.gaxis.value < -JoyAxisThreshold);

			int posIdx = axis * 2;
			int negIdx = axis * 2 + 1;

			if (posState != js.axisButtonState[posIdx]) {
				js.axisButtonState[posIdx] = posState;
				if (posState) js.axisPressed[posIdx] = true;
				dispatchGamepadInput(gpIdx, WormSettingsExtensions::gamepadAxisPositive(axis), posState, controller);
			}
			if (negState != js.axisButtonState[negIdx]) {
				js.axisButtonState[negIdx] = negState;
				if (negState) js.axisPressed[negIdx] = true;
				dispatchGamepadInput(gpIdx, WormSettingsExtensions::gamepadAxisNegative(axis), negState, controller);
			}
		}
		break;

		case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
		case SDL_EVENT_GAMEPAD_BUTTON_UP:
		{
			int gpIdx = findGamepadIndex(ev.gbutton.which);
			if (gpIdx < 0) break;
			Joystick& js = joysticks[gpIdx];
			int btn = ev.gbutton.button;
			bool state = ev.gbutton.down;
			js.btnState[btn] = state;
			if (state)
				js.btnPressed[btn] = true;
			dispatchGamepadInput(gpIdx, (uint32_t)btn, state, controller);
		}
		break;

		case SDL_EVENT_GAMEPAD_ADDED:
		{
			SDL_JoystickID id = ev.gdevice.which;
			// Only track up to 2 gamepads
			if (joysticks.size() < 2)
			{
				Joystick js;
				js.sdlGamepad = SDL_OpenGamepad(id);
				js.instanceId = id;
				js.clearState();
				joysticks.push_back(js);
			}
		}
		break;

		case SDL_EVENT_GAMEPAD_REMOVED:
		{
			SDL_JoystickID id = ev.gdevice.which;
			for (auto it = joysticks.begin(); it != joysticks.end(); ++it)
			{
				if (it->instanceId == id)
				{
					SDL_CloseGamepad(it->sdlGamepad);
					joysticks.erase(it);
					break;
				}
			}
		}
		break;

		default:
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

int Gfx::findGamepadIndex(SDL_JoystickID id)
{
	for (int i = 0; i < (int)joysticks.size(); ++i)
	{
		if (joysticks[i].instanceId == id)
			return i;
	}
	return -1;
}

int Gfx::findGamepadForPlayer(int playerIdx)
{
	if (!settings || playerIdx < 0 || playerIdx >= 2) return -1;
	WormSettings& ws = *settings->wormSettings[playerIdx];
	if (ws.inputDevice == WormSettingsExtensions::InputKeyboard) return -1;
	if (ws.gamepadName.empty()) return -1;

	// Collect indices of gamepads matching by name
	std::vector<int> candidates;
	for (int i = 0; i < (int)joysticks.size(); ++i)
	{
		char const* n = SDL_GetGamepadName(joysticks[i].sdlGamepad);
		if (!n || ws.gamepadName != n) continue;

		// Exact serial match is best
		if (!ws.gamepadSerial.empty())
		{
			char const* s = SDL_GetGamepadSerial(joysticks[i].sdlGamepad);
			if (s && ws.gamepadSerial == s)
				return i;
		}
		candidates.push_back(i);
	}

	if (candidates.empty()) return -1;

	// No serial match — resolve by position among same-name candidates.
	// If the other player also wants a gamepad with the same name,
	// give the first candidate to player 0 and the second to player 1.
	int otherPlayer = 1 - playerIdx;
	WormSettings& otherWs = *settings->wormSettings[otherPlayer];
	if (candidates.size() >= 2
	    && otherWs.inputDevice != WormSettingsExtensions::InputKeyboard
	    && otherWs.gamepadName == ws.gamepadName)
	{
		// Both players want same-named gamepad — split by player index
		return candidates[playerIdx < otherPlayer ? 0 : 1];
	}

	return candidates[0];
}

void Gfx::dispatchGamepadInput(int gpIdx, uint32_t gamepadKey, bool state, Controller* controller)
{
	if (gpIdx < 0 || gpIdx >= 2) return;

	// Start button acts as ESC for menu access
	if (gamepadKey == (uint32_t)SDL_GAMEPAD_BUTTON_START && state)
	{
		dosKeys[DkEscape] = true;
		if (controller)
			controller->onKey(DkEscape, true);
		return;
	}

	// Dispatch to the controller for the player who has this gamepad assigned
	if (controller)
	{
		for (int p = 0; p < 2; ++p)
		{
			WormSettings& ws = *settings->wormSettings[p];
			if (ws.inputDevice == WormSettingsExtensions::InputKeyboard)
				continue;
			if (findGamepadForPlayer(p) != gpIdx)
				continue;

			for (int c = 0; c < WormSettings::MaxControlEx; ++c)
			{
				if (ws.gamepadControls[c] == gamepadKey)
					controller->onKey(gamepadControlToExKey(p, c), state);
			}
		}
	}
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

std::string Gfx::getGamepadKeyName(uint32_t gamepadKey)
{
	if (WormSettingsExtensions::isGamepadAxis(gamepadKey))
	{
		int axis = (gamepadKey - WormSettingsExtensions::GamepadAxisBase) / 2;
		bool negative = (gamepadKey - WormSettingsExtensions::GamepadAxisBase) % 2;
		static char const* axisNames[] = {"LX", "LY", "RX", "RY", "LT", "RT"};
		std::string name = (axis < 6) ? axisNames[axis] : "A" + toString(axis);
		return name + (negative ? "-" : "+");
	}

	static char const* buttonNames[] = {
		"A", "B", "X", "Y",
		"Back", "Guide", "Start",
		"LS", "RS",
		"LB", "RB",
		"Up", "Down", "Left", "Right"
	};

	if (gamepadKey < 15)
		return buttonNames[gamepadKey];

	return "Btn" + toString(gamepadKey);
}

void Gfx::clearKeys()
{
	std::memset(dosKeys, 0, sizeof(dosKeys));
	exKeys.clear();
	for (auto& js : joysticks)
		js.clearState();
}

bool Gfx::testControlOnce(int control)
{
	// Check keyboard bindings for all player profiles (left, right, network)
	for(int p = 0; p < Settings::NumWormSettings; ++p)
	{
		if (settings->wormSettings[p]->inputDevice != WormSettingsExtensions::InputKeyboard)
			continue;
		uint32_t key = settings->extensions
			? settings->wormSettings[p]->controlsEx[control]
			: settings->wormSettings[p]->controls[control];
		if(testAnyKeyOnce(key))
			return true;
	}
	return false;
}

bool Gfx::testGamepadButtonOnce(int button)
{
	for (int gp = 0; gp < (int)joysticks.size(); ++gp)
	{
		if (joysticks[gp].btnPressed[button])
		{
			joysticks[gp].btnPressed[button] = false;
			return true;
		}
	}
	return false;
}

bool Gfx::testGamepadButton(int button)
{
	for (int gp = 0; gp < (int)joysticks.size(); ++gp)
	{
		if (joysticks[gp].btnState[button])
			return true;
	}
	return false;
}

// Map DPad button to left stick axis index (-1 if not a directional button)
static int dpadToAxisIndex(int dpadButton)
{
	switch (dpadButton)
	{
		case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return 0; // LEFTX positive
		case SDL_GAMEPAD_BUTTON_DPAD_LEFT:  return 1; // LEFTX negative
		case SDL_GAMEPAD_BUTTON_DPAD_DOWN:  return 2; // LEFTY positive
		case SDL_GAMEPAD_BUTTON_DPAD_UP:    return 3; // LEFTY negative
		default: return -1;
	}
}

bool Gfx::testGamepadDirOnce(int dpadButton)
{
	int axisIdx = dpadToAxisIndex(dpadButton);
	for (int gp = 0; gp < (int)joysticks.size(); ++gp)
	{
		if (joysticks[gp].btnPressed[dpadButton])
		{
			joysticks[gp].btnPressed[dpadButton] = false;
			if (axisIdx >= 0) joysticks[gp].axisPressed[axisIdx] = false;
			return true;
		}
		if (axisIdx >= 0 && joysticks[gp].axisPressed[axisIdx])
		{
			joysticks[gp].axisPressed[axisIdx] = false;
			joysticks[gp].btnPressed[dpadButton] = false;
			return true;
		}
	}
	return false;
}

bool Gfx::testGamepadDir(int dpadButton)
{
	int axisIdx = dpadToAxisIndex(dpadButton);
	for (int gp = 0; gp < (int)joysticks.size(); ++gp)
	{
		if (joysticks[gp].btnState[dpadButton])
			return true;
		if (axisIdx >= 0 && joysticks[gp].axisButtonState[axisIdx])
			return true;
	}
	return false;
}

bool Gfx::testControl(int control)
{
	// Check keyboard bindings for all player profiles (left, right, network)
	for(int p = 0; p < Settings::NumWormSettings; ++p)
	{
		if (settings->wormSettings[p]->inputDevice != WormSettingsExtensions::InputKeyboard)
			continue;
		uint32_t key = settings->extensions
			? settings->wormSettings[p]->controlsEx[control]
			: settings->wormSettings[p]->controls[control];
		if(testAnyKey(key))
			return true;
	}
	return false;
}

void Gfx::releaseControl(int control)
{
	for(int p = 0; p < Settings::NumWormSettings; ++p)
	{
		uint32_t key = settings->extensions
			? settings->wormSettings[p]->controlsEx[control]
			: settings->wormSettings[p]->controls[control];
		releaseAnyKey(key);
	}
}

void Gfx::preparePalette(SDL_PixelFormatDetails const* format, SDL_Palette const* palette, Color realPal[256], uint32_t (&pal32)[256])
{
	for(int i = 0; i < 256; ++i)
	{
		pal32[i] = SDL_MapRGB(format, palette, realPal[i].r, realPal[i].g, realPal[i].b);
	}
}

void Gfx::menuFlip(bool quitting)
{
	if (playRenderer.fadeValue < 32 && !quitting)
		++playRenderer.fadeValue;
	if (singleScreenRenderer.fadeValue < 32 && !quitting)
		++singleScreenRenderer.fadeValue;

	++menuCycles;
	playRenderer.pal = playRenderer.origpal;
	playRenderer.pal.rotateFrom(playRenderer.origpal, 168, 174, menuCycles);
	playRenderer.pal.setWormColours(*settings);
	if (curMenu == &playerMenu && playerMenu.ws == settings->wormSettings[Settings::NetworkPlayerIdx])
		playRenderer.pal.setWormColour(0, *playerMenu.ws);
	playRenderer.pal.fade(playRenderer.fadeValue);
	singleScreenRenderer.pal = singleScreenRenderer.origpal;
	singleScreenRenderer.pal.rotateFrom(singleScreenRenderer.origpal, 168, 174, menuCycles);
	singleScreenRenderer.pal.setWormColours(*settings);
	if (curMenu == &playerMenu && playerMenu.ws == settings->wormSettings[Settings::NetworkPlayerIdx])
		singleScreenRenderer.pal.setWormColour(0, *playerMenu.ws);
	singleScreenRenderer.pal.fade(singleScreenRenderer.fadeValue);
	flip();
}

void Gfx::draw(SDL_Surface& surface, SDL_Texture& texture, SDL_Renderer& sdlRenderer, Renderer& renderer)
{
	gvl::rect updateRect;
	Color realPal[256];
	renderer.pal.activate(realPal);
	int offsetX, offsetY;
	int mag = fitScreen(surface.w, surface.h,
						renderer.renderResX, renderer.renderResY, offsetX, offsetY);

	gvl::rect newRect(offsetX, offsetY, renderer.renderResX * mag, renderer.renderResY * mag);

	if(mag != prevMag)
	{
		// Clear background if magnification is decreased to
		// avoid leftovers.
		SDL_FillSurfaceRect(&surface, 0, 0);
		updateRect = lastUpdateRect | newRect;
	}
	else
		updateRect = newRect;
	prevMag = mag;

	std::size_t destPitch = surface.pitch;
	std::size_t srcPitch = renderer.bmp.pitch;

	SDL_PixelFormatDetails const* formatDetails = SDL_GetPixelFormatDetails(surface.format);
	PalIdx* dest = reinterpret_cast<PalIdx*>(surface.pixels) + offsetY * destPitch + offsetX * formatDetails->bytes_per_pixel;
	PalIdx* src = renderer.bmp.pixels;

	uint32_t pal32[256];
	preparePalette(formatDetails, NULL, realPal, pal32);
	scaleDraw(src, renderer.renderResX, renderer.renderResY, srcPitch, dest, destPitch, mag, pal32);

	SDL_UpdateTexture(&texture, NULL, surface.pixels, surface.w * 4);
	SDL_RenderClear(&sdlRenderer);
	SDL_RenderTexture(&sdlRenderer, &texture, NULL, NULL);
	SDL_RenderPresent(&sdlRenderer);

	lastUpdateRect = updateRect;
}

void Gfx::flip()
{
	// draw into the play window. This uses either the normal split screen renderer
	// or the single screen renderer if this is a replay and single screen replay
	// is turned on
	draw(*sdlDrawSurface, *sdlTexture, *sdlRenderer, *primaryRenderer);
	if (settings->spectatorWindow)
	{
		draw(*sdlSpectatorDrawSurface, *sdlSpectatorTexture, *sdlSpectatorRenderer, singleScreenRenderer);
	}

	static unsigned int const delay = 14u;

	auto wantedTime = lastFrame + delay;

	while(true)
	{
		auto now = SDL_GetTicks();
		if(now >= wantedTime)
			break;

		SDL_Delay((uint32_t)(wantedTime - now));
	}

	lastFrame = wantedTime;
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

	Common& common;
	WormSettings& ws;
};



struct LevelSelectBehavior : ItemBehavior
{
	LevelSelectBehavior(Common& common)
	: common(common)
	{
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

	Common& common;
};

struct OptionsSaveBehavior : ItemBehavior
{
	OptionsSaveBehavior(Common& common)
	: common(common)
	{
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


void PlayerMenu::drawItemOverlay(Common& common, MenuItem& item, int x, int y, bool selected, bool disabled)
{
	if(item.id >= PlayerMenu::PlRed && item.id <= PlayerMenu::PlBlue) //Color settings
	{
		int rgbcol = item.id - PlayerMenu::PlRed;

		if(selected)
		{
			drawRoundedBox(gfx.playRenderer.bmp, x + 24, y, 168, 7, ws->rgb[rgbcol] - 1);
		}
		else // CE98
		{
			drawRoundedBox(gfx.playRenderer.bmp, x + 24, y, 0, 7, ws->rgb[rgbcol] - 1);
		}

		fillRect(gfx.playRenderer.bmp, x + 25, y + 1, ws->rgb[rgbcol], 5, ws->color);
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

		case PlInput:
			return new InputDeviceBehavior(common, *ws);

		case PlUp: // D2AB
		case PlDown:
		case PlLeft:
		case PlRight:
		case PlFire:
		case PlChange:
		case PlJump:
			return new KeyBehavior(common, ws->controls[item.id - PlUp], ws->controlsEx[item.id - PlUp], ws->gamepadControls[item.id - PlUp], ws->inputDevice, gfx.settings->extensions);

		case PlDig: // Controls Extension
			return new KeyBehavior(common, ws->controlsEx[item.id - PlUp], ws->controlsEx[item.id - PlUp], ws->gamepadControls[item.id - PlUp], ws->inputDevice, gfx.settings->extensions);


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

void Gfx::initFrameStepping()
{
	tcChangeRequested_ = false;

	controller.reset(new LocalController(common, settings));

	{
		Level newLevel(*common);
		newLevel.generateFromSettings(*common, *settings, rand);
		controller->swapLevel(newLevel);
	}

	controller->currentGame()->focus(this->playRenderer);
	controller->currentGame()->focus(this->singleScreenRenderer);

	// Draw the initial game state so the menu has a proper background
	playRenderer.clear();
	controller->draw(this->playRenderer, false);
	singleScreenRenderer.clear();
	controller->draw(this->singleScreenRenderer, true);

	// Push the initial menu state
	auto menuState = std::make_unique<MainMenuState>();
	menuStatePtr_ = menuState.get();
	stateStack.push(std::move(menuState), this);
}

bool Gfx::runOneFrame()
{
	if (stateStack.empty())
		return false;

	// Poll events
	SDL_Event ev;
	keyBufPtr = keyBuf;
	while (SDL_PollEvent(&ev))
	{
		if (ev.type == SDL_EVENT_QUIT)
			return false;
		if (ev.type == SDL_EVENT_KEY_DOWN
			&& ev.key.scancode == SDL_SCANCODE_F4
			&& (ev.key.mod & SDL_KMOD_ALT))
			return false;
		stateStack.handleEvent(ev);
	}

	// Capture menu selection before update() might pop and destroy the state
	int menuSelection = menuStatePtr_ ? menuStatePtr_->selection() : -1;
	bool menuFadingOut = menuStatePtr_ && menuStatePtr_->isFadingOut();

	if (!stateStack.update())
	{
		// Top state popped. Determine what to do next.
		if (menuSelection >= 0)
		{
			menuStatePtr_ = nullptr;

			if (menuSelection == MainMenu::MaQuit)
				return false;

			if (menuSelection == MainMenu::MaTc)
			{
				tcChangeRequested_ = true;
				controller.reset();
				return false;
			}

			// Handle new game / resume / replay selection
			if (menuSelection == MainMenu::MaNewGame)
			{
				netSession.reset();
				std::unique_ptr<Controller> newController(new LocalController(common, settings));
				Level* oldLevel = controller->currentLevel();

				if (oldLevel
					&& !settings->regenerateLevel
					&& settings->randomLevel == oldLevel->oldRandomLevel
					&& settings->levelFile == oldLevel->oldLevelFile)
				{
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
			else if (menuSelection == MainMenu::MaResumeGame)
			{
				if (controller->isReplay())
					primaryRenderer = &singleScreenRenderer;
			}
			else if (menuSelection == MainMenu::MaReplay)
			{
				if (settings->singleScreenReplay)
					primaryRenderer = &singleScreenRenderer;
			}
			else if (menuSelection == MainMenu::MaHostGame)
			{
				stateStack.push(std::make_unique<NetConnectState>(
					NetSession::Host, "", gfx.onlinePort), this);
				return true;
			}
			else if (menuSelection == MainMenu::MaJoinGame)
			{
				// Parse address — support "host:port" and "[ipv6]:port" formats
				std::string addr = std::move(pendingNetAddress);
				uint16_t port = gfx.onlinePort;

				if (!addr.empty() && addr[0] == '[') {
					// IPv6 bracket notation: [::1]:port
					auto closeBracket = addr.find(']');
					if (closeBracket != std::string::npos) {
						std::string ip6 = addr.substr(1, closeBracket - 1);
						if (closeBracket + 1 < addr.size() && addr[closeBracket + 1] == ':') {
							try {
								port = static_cast<uint16_t>(std::stoi(addr.substr(closeBracket + 2)));
							} catch (...) {
								// Malformed port, keep default
							}
						}
						addr = ip6;
					}
				} else {
					// IPv4 or hostname: check for last colon
					auto lastColon = addr.rfind(':');
					if (lastColon != std::string::npos) {
						// Only treat as port separator if there's at most one colon
						// (multiple colons = bare IPv6 without port)
						auto firstColon = addr.find(':');
						if (firstColon == lastColon) {
							try {
								port = static_cast<uint16_t>(std::stoi(addr.substr(lastColon + 1)));
							} catch (...) {
								// Malformed port, keep default
							}
							addr = addr.substr(0, lastColon);
						}
					}
				}

				stateStack.push(std::make_unique<NetConnectState>(
					NetSession::Client, std::move(addr), port), this);
				return true;
			}
			else if (menuSelection == MainMenu::MaHostOnline)
			{
				stateStack.push(std::make_unique<OnlineConnectState>(
					NetSession::Host), this);
				return true;
			}
			else if (menuSelection == MainMenu::MaJoinOnline)
			{
				std::string code = std::move(pendingNetAddress);
				stateStack.push(std::make_unique<OnlineConnectState>(
					NetSession::Client, std::move(code)), this);
				return true;
			}

			// Push game state
			stateStack.push(std::make_unique<GamePlayState>(), this);
		}
		else
		{
			// Game state finished — go back to menu
			netSession.reset();
			primaryRenderer = &playRenderer;
			controller->unfocus();
			clearKeys();

			// Draw one frame so the menu background captures the final game state
			playRenderer.clear();
			controller->draw(this->playRenderer, false);
			singleScreenRenderer.clear();
			controller->draw(this->singleScreenRenderer, true);

			auto newMenu = std::make_unique<MainMenuState>();
			menuStatePtr_ = newMenu.get();
			stateStack.push(std::move(newMenu), this);
		}
		return true;
	}

	stateStack.draw();

	// Flip: game states use plain flip(), menu states use menuFlip()
	auto* top = stateStack.top();
	if (top && !top->wantsMenuFlip())
	{
		++menuCycles;
		flip();
	}
	else
	{
		menuFlip(menuFadingOut);
	}

	return true;
}

void Gfx::mainLoop()
{
	initFrameStepping();

#if OPENLIERO_EMSCRIPTEN
	emscripten_set_main_loop_arg([](void* arg) {
		Gfx* self = static_cast<Gfx*>(arg);
		if (!self->runOneFrame())
		{
			if (self->tcChangeRequested())
			{
				self->initFrameStepping();
			}
			else
			{
				self->controller.reset();
				emscripten_cancel_main_loop();
			}
		}
	}, this, 0, true);
#else
	while (true)
	{
		while (runOneFrame())
		{
		}

		if (!tcChangeRequested())
			break;

		// TC was changed (common reloaded by TcSelectorState) — reinitialize
		initFrameStepping();
	}

	controller.reset();
#endif
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
	playRenderer.bmp.copy(frozenScreen);

	mainMenu.draw(*common, playRenderer, curMenu != &mainMenu, -1, true);
}

void Gfx::drawSpectatorInfo()
{
	Common& common = *this->common;
	int centerX = singleScreenRenderer.renderResX / 2;
	int centerY = singleScreenRenderer.renderResY / 4;

	singleScreenRenderer.bmp.copy(frozenSpectatorScreen);
	if(settings->levelFile.empty())
	{
		common.font.drawCenteredText(singleScreenRenderer.bmp, LS(LevelRandom), centerX, centerY - 32, 7, 2);
	}
	else
	{
		auto levelName = getBasename(getLeaf(gfx.settings->levelFile));
		common.font.drawCenteredText(singleScreenRenderer.bmp, LS(LevelIs1) + levelName + LS(LevelIs2), centerX, centerY - 32, 7, 2);
	}

	std::string vsText = settings->wormSettings[0]->name + " vs " + settings->wormSettings[1]->name;
	// put worm color boxes on a nice spot even if no player names have been entered
	int textSize = std::max(common.font.getDims(vsText) * 2, 48);
	common.font.drawCenteredText(singleScreenRenderer.bmp, vsText, centerX, centerY, 7, 2);
	fillRect(singleScreenRenderer.bmp, centerX - (textSize / 2) - 1, centerY + 23 - 1, 16, 16, 7);
	fillRect(singleScreenRenderer.bmp, centerX - textSize / 2, centerY + 23, 14, 14, settings->wormSettings[0]->color);
	fillRect(singleScreenRenderer.bmp, centerX + (textSize / 2) - 16 - 1, centerY + 23 - 1, 16, 16, 7);
	fillRect(singleScreenRenderer.bmp, centerX + textSize / 2 - 16, centerY + 23, 14, 14, settings->wormSettings[1]->color);

	if (controller->running())
	{
		common.font.drawCenteredText(singleScreenRenderer.bmp, "PAUSED", centerX, centerY + 48, 7, 2);
	}
	else
	{
		common.font.drawCenteredText(singleScreenRenderer.bmp, "SETUP", centerX, centerY + 48, 7, 2);
	}
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
