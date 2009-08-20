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
//#include <iostream>
#include <cstdio>
#include <memory>
#include "controller.hpp"

#include "gfx/macros.hpp"

/*
ds:0000 is 0x 1AE80
*/

Gfx gfx;





void SpriteSet::read(FILE* f, int width, int height, int count)
{
	assert(width == height); // We only support rectangular sprites right now
	
	this->width = width;
	this->height = height;
	this->spriteSize = width * height;
	this->count = count;
	
	int amount = spriteSize * count;
	data.resize(amount);
	
	std::vector<PalIdx> temp(amount);
	
	checkedFread(&temp[0], 1, amount, f);
	
	PalIdx* dest = &data[0];
	PalIdx* src = &temp[0];
	
	for(int i = 0; i < count; i++)
	{
		for(int x = 0; x < width; ++x)
		{
			for(int y = 0; y < height; ++y)
			{
				dest[x + y*width] = src[y];
			}
			
			src += height;
		}
		
		dest += spriteSize;
	}
}

void SpriteSet::allocate(int width, int height, int count)
{
	this->width = width;
	this->height = height;
	this->spriteSize = width * height;
	this->count = count;
	
	int amount = spriteSize * count;
	data.resize(amount);
}

struct ArrayEnumBehavior : EnumBehavior
{
	template<int N>
	ArrayEnumBehavior(Common& common, uint32_t& v, std::string const (&arr)[N], bool brokenEnter = false)
	: EnumBehavior(common, v, 0, N-1, brokenEnter)
	, arr(arr)
	{
	}
		
	void onUpdate(Menu& menu, int item)
	{
		MenuItem& i = menu.items[item];
		i.value = arr[v];
		i.hasValue = true;
	}
	
	std::string const* arr;
};

struct KeyBehavior : ArrayEnumBehavior
{
	KeyBehavior(Common& common, uint32_t& v)
	: ArrayEnumBehavior(common, v, common.texts.keyNames)
	{
	}
	
	int onEnter(Menu& menu, int item)
	{
		sfx.play(27);
		
		SDL_keysym key(gfx.waitForKey());
		
		if(key.sym != SDLK_ESCAPE)
		{
			uint32_t k = SDLToDOSKey(key.sym);
			if(k)
			{
				v = k;
				onUpdate(menu, item);
			}
		}
		
		gfx.clearKeys();
		return -1;
	}
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
		sfx.play(27);
		
		ws.randomName = false;
		gfx.inputString(ws.name, 20, 275, 20);
		
		if(ws.name.empty())
		{
			Settings::generateName(ws);
		}
		sfx.play(27);
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
		sfx.play(27);
		
		int x, y;
		if(!menu.itemPosition(item, x, y))
			return -1;
			
		x += menu.valueOffsetX + 2;
		
		if(saveAs)
		{
			std::string name;
			if(gfx.inputString(name, 30, x, y) && !name.empty())
				ws.saveProfile(name);
				
			sfx.play(27);
		}
		else
			ws.saveProfile(ws.profileName);
		
		onUpdate(menu, item);
		return -1;
	}
	
	void onUpdate(Menu& menu, int item)
	{
		if(saveAs)
		{
			menu.items[item].value = ws.profileName;
			menu.items[item].hasValue = true;
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
, screen(0)
, back(0)
, frozenScreen(320 * 200)
, running(true)
, fullscreen(false)
, fadeValue(0)
, menuCyclic(0)
, windowW(320)
, windowH(200)
, prevMag(0)

{
	clearKeys();
}

void Gfx::init()
{
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
	SDL_EnableUNICODE(1);
	SDL_WM_SetCaption("Liero 1.35b2", 0);
	SDL_ShowCursor(SDL_DISABLE);
	lastFrame = SDL_GetTicks();
	
	screen = SDL_CreateRGBSurface(SDL_SWSURFACE, 320, 200, 8, 0, 0, 0, 0);

	screenPixels = static_cast<unsigned char*>(screen->pixels);
	screenPitch = screen->pitch;
}

void Gfx::setVideoMode()
{
	int bitDepth = 8;
	if(settings->depth32)
		bitDepth = 32;
	
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
}

void Gfx::loadPalette()
{
	origpal = common->exepal;
	pal = origpal;
}

void Gfx::loadMenus()
{
	FILE* exe = openLieroEXE();
	
	fseek(exe, 0x1B08A, SEEK_SET);
	mainMenu.readItems(exe, 14, 4, true);
			
	fseek(exe, 0x1B0C2, SEEK_SET);
	settingsMenu.readItems(exe, 21, 15, false, 48, 7);
	settingsMenu.valueOffsetX = 100;
	
	//settingsMenu.addItem(MenuItem(48, 7, common->texts.gameModeSpec[0]), 1);
	settingsMenu.items[Settings::SiLives].string = common->texts.gameModeSpec[0];
	settingsMenu.addItem(MenuItem(48, 7, common->texts.gameModeSpec[1]), Settings::SiTimeToLose);
	settingsMenu.addItem(MenuItem(48, 7, common->texts.gameModeSpec[2]), Settings::SiFlagsToWin);
	
	//settingsMenuValues.items.assign(12, MenuItem(48, 7, ""));
	// First 14 items have values
	for(int i = 0; i < Settings::SiPlayer1Options; ++i)
	{
		settingsMenu.items[i].hasValue = true;
	}
	
	fseek(exe, 0x1B210, SEEK_SET);
	playerMenu.readItems(exe, 13, 13, false, 48, 7);
	playerMenu.valueOffsetX = 95;
	
	playerMenu.addItem(MenuItem(3, 7, "SAVE PROFILE"));
	playerMenu.addItem(MenuItem(3, 7, "SAVE PROFILE AS..."));
	playerMenu.addItem(MenuItem(3, 7, "LOAD PROFILE"));
	
	for(int i = 0; i < 13; ++i)
	{
		playerMenu.items[i].hasValue = true;
	}
	
	hiddenMenu.addItem(MenuItem(48, 7, "Extensions"));
	hiddenMenu.addItem(MenuItem(48, 7, "Record replays"));
	hiddenMenu.addItem(MenuItem(48, 7, "Load replay..."));
	hiddenMenu.addItem(MenuItem(48, 7, "PowerLevel palettes"));
	hiddenMenu.addItem(MenuItem(48, 7, "Scaling filter"));
	hiddenMenu.addItem(MenuItem(48, 7, "Fullscreen width"));
	hiddenMenu.addItem(MenuItem(48, 7, "Fullscreen height"));
	hiddenMenu.addItem(MenuItem(48, 7, "TESTING 32-bit mode"));
	hiddenMenu.valueOffsetX = 100;
}

void Gfx::updateSettingsMenu()
{
	settingsMenu.items[Settings::SiGameMode].value = common->texts.gameModes[settings->gameMode];
	
	settingsMenu.setVisibility(Settings::SiLives, false);
	settingsMenu.setVisibility(Settings::SiTimeToLose, false);
	settingsMenu.setVisibility(Settings::SiFlagsToWin, false);
	
	switch(settings->gameMode)
	{
		case Settings::GMKillEmAll:
			settingsMenu.setVisibility(Settings::SiLives, true);
		break;
		
		case Settings::GMGameOfTag:
			settingsMenu.setVisibility(Settings::SiTimeToLose, true);
		break;
		
		case Settings::GMCtF:
		case Settings::GMSimpleCtF:
			settingsMenu.setVisibility(Settings::SiFlagsToWin, true);
		break;
	}
}


void Gfx::processEvent(SDL_Event& ev, Controller* controller)
{
	switch(ev.type)
	{
		case SDL_KEYDOWN:
		{
		
			SDLKey s = ev.key.keysym.sym;
			/*
			gfx.keys[s] = true;
			*/
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
				fullscreen = !fullscreen;
				if(fullscreen)
				{
					// Try lowest resolution
					windowW = 320;
					windowH = 200;
				}
				setVideoMode();
			}
			else if(s == SDLK_F6)
			{
				if(windowW >= 640 && windowH >= 480)
				{
					windowW = 320;
					windowH = 200;
				}
				else
				{
					windowW = 640;
					windowH = 480;
				}
				setVideoMode();
			}
		}
		break;
		
		case SDL_KEYUP:
		{/*
			gfx.keys[ev.key.keysym.sym] = false;
			*/
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
	}
}

void Gfx::process(Controller* controller)
{
	SDL_Event ev;
	while(SDL_PollEvent(&ev))
	{
		processEvent(ev, controller);
	}
	
	processReader();
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

void Gfx::clearKeys()
{
	//std::memset(keys, 0, sizeof(keys));
	std::memset(dosKeys, 0, sizeof(dosKeys));
}

void preparePalette(SDL_PixelFormat* format, SDL_Palette* pal, uint32_t (&pal32)[256])
{
	for(int i = 0; i < 256; ++i)
	{
		pal32[i] = SDL_MapRGB(format, pal->colors[i].r, pal->colors[i].g, pal->colors[i].b);		 
	}
}

int Gfx::fitScreen(int backW, int backH, int scrW, int scrH, int& offsetX, int& offsetY)
{
	int mag = 1;
	
	while(scrW*mag <= backW
	   && scrH*mag <= backH)
	   ++mag;
	   
	--mag; // mag was the first that didn't fit
	
	if(settings->scaleFilter == Settings::SfScale2X)
	{
		mag = std::min(mag, 2);
	}
	
	scrW *= mag;
	scrH *= mag;
	
	offsetX = backW/2 - scrW/2;
	offsetY = backH/2 - scrH/2;
	   
	return mag; 
}

void Gfx::flip()
{
	gvl::rect updateRect;
	pal.activate();
	if(screen != back)
	{
		int offsetX, offsetY;
		int mag = fitScreen(back->w, back->h, screen->w, screen->h, offsetX, offsetY);
		
		gvl::rect newRect(offsetX, offsetY, screen->w * mag, screen->h * mag);
		
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
		std::size_t srcPitch = screenPitch;
		
		PalIdx* dest = reinterpret_cast<PalIdx*>(back->pixels) + offsetY * destPitch + offsetX * back->format->BytesPerPixel;
		PalIdx* src = screenPixels;
		
		
		if(back->format->BitsPerPixel == 8)
		{
			
			if(mag == 1)
			{
				for(int y = 0; y < 200; ++y)
				{
					PalIdx* line = src + y*srcPitch;
					PalIdx* destLine = dest + y*destPitch;
					
					std::memcpy(destLine, line, 320);
	#if 0
					for(int x = 0; x < 320; ++x)
					{
						PalIdx pix = src[y*srcPitch + x];
						dest[y*destPitch + x] = pix;
					}
	#endif
				}
			}
			else if(mag == 2)
			{
	#if 1

				if(settings->scaleFilter == Settings::SfNearest)
				{
		
					for(int y = 0; y < 200; ++y)
					{
						PalIdx* line = src + y*srcPitch;
						PalIdx* destLine = dest + 2*y*destPitch;
						
	#if 0
						for(int x = 0; x < 320; ++x)
						{
							PalIdx pix = *line++;
							destLine[0] = pix;
							destLine[1] = pix;
							destLine[destPitch] = pix;
							destLine[destPitch + 1] = pix;
							
							destLine += 2;
						}
	#else
						// NOTE! This only works on a little-endian machine that allows unaligned access
						for(int x = 0; x < 320/4; ++x)
						{
							// !arch NOTE! Unaligned access
							uint32_t pix = *reinterpret_cast<uint32_t*>(line);
							line += 4;
							
							uint32_t a = (pix & 0xff000000);
							uint32_t b = (pix & 0x00ff0000) >> 8;
							uint32_t c = (pix & 0x0000ff00) << 16;
							uint32_t d = (pix & 0x000000ff) << 8;
							
							uint32_t A = a | b;
							uint32_t C = c | d;
							
							A |= A >> 8;
							C |= C >> 8;
							
							uint32_t* dest32T = reinterpret_cast<uint32_t*>(destLine);
							uint32_t* dest32B = reinterpret_cast<uint32_t*>(destLine + destPitch);
							
							// !arch NOTE! Assumes little-endian, C and A should be swapped if big-endian
							dest32T[0] = C;
							dest32T[1] = A;
							dest32B[0] = C;
							dest32B[1] = A;
							
							destLine += 8;
						}
	#endif
					}
					
				}
				else if(settings->scaleFilter == Settings::SfScale2X)
				{
					#define DECL int downOffset = destPitch ; SCALE2X_DECL
					FILTER_X(dest, 2*destPitch, src, srcPitch, 320, 200, 1, 2, SCALE2X, DECL, READER_8, WRITER_2X_8);
					#undef DECL
				}
	#endif
			}
			else if(mag > 2)
			{
				for(int y = 0; y < 200; ++y)
				{
					PalIdx* line = src + y*srcPitch;
					int destMagPitch = mag*destPitch;
					PalIdx* destLine = dest + y*destMagPitch;
					
					for(int x = 0; x < 320/4 - 1; ++x)
					{
						uint32_t pix = *reinterpret_cast<uint32_t*>(line);
						line += 4;
						
						uint32_t a = pix >> 24;
						uint32_t b = pix & 0x00ff0000;
						uint32_t c = pix & 0x0000ff00;
						uint32_t d = pix & 0x000000ff;
						
						a |= (a << 8);
						b |= (b << 8);
						c |= (c >> 8);
						d |= (d << 8);
						
						a |= (a << 16);
						b |= (b >> 16);
						c |= (c << 16);
						d |= (d << 16);
						
						// !arch
						#define WRITE_BLOCK(C) \
						do { \
							int i = mag; \
							while(i >= 4) { \
								for(int y = 0; y < destMagPitch; y += destPitch) { \
									uint32_t* dest32 = reinterpret_cast<uint32_t*>(destLine + y); \
									*dest32 = (C); \
								} \
								destLine += 4; \
								i -= 4; \
							} \
							if(i > 0) { \
								for(int y = 0; y < destMagPitch; y += destPitch) { \
									uint32_t* dest32 = reinterpret_cast<uint32_t*>(destLine + y); \
									*dest32 = (C); \
								} \
								destLine += i; \
							} \
						} while(0)
						
						// !arch
						WRITE_BLOCK(d);
						WRITE_BLOCK(c);
						WRITE_BLOCK(b);
						WRITE_BLOCK(a);
						
						#undef WRITE_BLOCK
						
					}
					
					for(int x = 0; x < 4; ++x)
					{
						PalIdx pix = *line++;
						for(int dy = 0; dy < destMagPitch; dy += destPitch)
						{
							for(int dx = 0; dx < mag; ++dx)
							{
								destLine[dy + dx] = pix;
							}
						}
						destLine += mag;
					}
				}
			}
		}
		else if(back->format->BitsPerPixel == 32)
		{
			uint32_t pal32[256];
			preparePalette(back->format, screen->format->palette, pal32);
			
			if(mag == 1)
			{
				for(int y = 0; y < 200; ++y)
				{
					PalIdx* line = src + y*srcPitch;
					uint32_t* destLine = reinterpret_cast<uint32_t*>(dest + y*destPitch);
					
					for(int x = 0; x < 320; ++x)
					{
						PalIdx pix = *line++;
						*destLine++ = pal32[pix];
					}
				}
			}
			else if(settings->scaleFilter == Settings::SfScale2X)
			{
				#define DECL int downOffset = destPitch ; SCALE2X_DECL
				#define PALREADER_8(x, src) do { \
					x = pal32[*(src)]; \
				} while(0)
				
				#define WRITE32(p, v) *reinterpret_cast<uint32_t*>(p) = (v)

				#define WRITER_2X_32(dest) do { \
					uint8_t* pix_2x_dest_ = dest; \
					WRITE32(pix_2x_dest_, R1); \
					WRITE32(pix_2x_dest_+4, R2); \
					WRITE32(pix_2x_dest_+downOffset, R3); \
					WRITE32(pix_2x_dest_+downOffset+4, R4); \
				} while(0)
				FILTER_X(dest, 2*destPitch, src, srcPitch, 320, 200, 1, 2*4, SCALE2X, DECL, PALREADER_8, WRITER_2X_32);
				#undef DECL
			}
			else
			{
				if(mag > 1)
				{
					for(int y = 0; y < 200; ++y)
					{
						PalIdx* line = src + y*srcPitch;
						int destMagPitch = mag*destPitch;
						uint8_t* destLine = dest + y*destMagPitch;
						
						for(int x = 0; x < 320/4; ++x)
						{
							uint32_t pix = *reinterpret_cast<uint32_t*>(line);
							line += 4;
							
							uint32_t a = pal32[pix >> 24];
							uint32_t b = pal32[(pix & 0x00ff0000) >> 16];
							uint32_t c = pal32[(pix & 0x0000ff00) >> 8];
							uint32_t d = pal32[pix & 0x000000ff];
							
							//uint32_t* destLine32 = reinterpret_cast<uint32_t*>(destLine);
							
							for(int dx = 0; dx < mag; ++dx)
							{
								for(int dy = 0; dy < destMagPitch; dy += destPitch)
								{
									*reinterpret_cast<uint32_t*>(destLine + dy) = d;
								}
								destLine += 4;
							}
							for(int dx = 0; dx < mag; ++dx)
							{
								for(int dy = 0; dy < destMagPitch; dy += destPitch)
								{
									*reinterpret_cast<uint32_t*>(destLine + dy) = c;
								}
								destLine += 4;
							}
							for(int dx = 0; dx < mag; ++dx)
							{
								for(int dy = 0; dy < destMagPitch; dy += destPitch)
								{
									*reinterpret_cast<uint32_t*>(destLine + dy) = b;
								}
								destLine += 4;
							}
							for(int dx = 0; dx < mag; ++dx)
							{
								for(int dy = 0; dy < destMagPitch; dy += destPitch)
								{
									*reinterpret_cast<uint32_t*>(destLine + dy) = a;
								}
								destLine += 4;
							}
						}
					}
				}
			}
		}
	}
	
	//if(fullscreen)
		SDL_Flip(back);
	/*else
		SDL_UpdateRect(back, updateRect.x1, updateRect.y1, updateRect.width(), updateRect.height());*/
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
		
		lastFrame = wantedTime;
		while((SDL_GetTicks() - lastFrame) > delay)
			lastFrame += delay;
	}
	else
		SDL_Delay(0);
}



void Gfx::clear()
{
	SDL_FillRect(screen, 0, 0);
}

void fillRect(int x, int y, int w, int h, int colour)
{
	SDL_Rect rect;
	rect.x = x;
	rect.y = y;
	rect.w = w;
	rect.h = h;
	SDL_FillRect(gfx.screen, &rect, colour);
}

void playChangeSound(int change)
{
	if(change > 0)
	{
		sfx.play(25);
	}
	else
	{
		sfx.play(26);
	}
}

void resetLeftRight()
{
/*
	gfx.keys[SDLK_LEFT] = false;
	gfx.keys[SDLK_RIGHT] = false;*/
	
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
		sfx.play(27);
		gfx.selectLevel();
		sfx.play(27);
		onUpdate(menu, item);
		return -1;
	}
	
	void onUpdate(Menu& menu, int item)
	{
		std::string levelPath = joinPath(lieroEXERoot, gfx.settings->levelFile + ".lev");
		if(!gfx.settings->randomLevel && fileExists(levelPath))
		{
			menu.items[Settings::SiLevel].value = '"' + gfx.settings->levelFile + '"';
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
		sfx.play(27);
		gfx.selectProfile(ws);
		sfx.play(27);
		menu.updateItems(common);
		return -1;
	}
		
	Common& common;
	WormSettings& ws;
};

struct WeaponOptionsBehavior : ItemBehavior
{
	int onEnter(Menu& menu, int item)
	{
		sfx.play(27);
		gfx.weaponOptions();
		sfx.play(27);
		return -1;
	}
};

struct PlayerSettingsBehavior : ItemBehavior
{
	PlayerSettingsBehavior(int player)
	: player(player)
	{
	}
	
	int onEnter(Menu& menu, int item)
	{
		sfx.play(27);
		gfx.playerSettings(player);
		return -1;
	}
	
	int player;
};


struct ReplaySelectBehavior : ItemBehavior
{
	int onEnter(Menu& menu, int item)
	{
		sfx.play(27);
		int ret = gfx.selectReplay();
		sfx.play(27);
		return ret;
	}
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
		case Settings::SiShadows:
			return new BooleanSwitchBehavior(common, gfx.settings->shadow);
		case Settings::SiScreenSync:
			return new BooleanSwitchBehavior(common, gfx.settings->screenSync);
		case Settings::SiLoadChange:
			return new BooleanSwitchBehavior(common, gfx.settings->loadChange);
		
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
		{
			TimeBehavior* ret = new TimeBehavior(common, gfx.settings->timeToLose, 60, 3600, 10);
			ret->allowEntry = false;
			return ret;
		}
		case Settings::SiFlagsToWin:
			return new IntegerBehavior(common, gfx.settings->flagsToWin, 1, 999, 1);
		
		case Settings::SiLevel:
			return new LevelSelectBehavior(common);
			
		case Settings::SiGameMode:
			return new ArrayEnumBehavior(common, gfx.settings->gameMode, common.texts.gameModes);
		case Settings::SiWeaponOptions:
			return new WeaponOptionsBehavior();
		case Settings::SiPlayer1Options:
			return new PlayerSettingsBehavior(0);
		case Settings::SiPlayer2Options:
			return new PlayerSettingsBehavior(1);
		
		default:
			return Menu::getItemBehavior(common, item);
	}
}

struct ExtensionsSwitchBehavior : BooleanSwitchBehavior
{
	ExtensionsSwitchBehavior(Common& common, bool& v)
	: BooleanSwitchBehavior(common, v)
	{
	}
	
	void onUpdate(Menu& menu, int item)
	{
		BooleanSwitchBehavior::onUpdate(menu, item);
		
		gfx.updateExtensions(v);
	}
};

struct Depth32Behavior : BooleanSwitchBehavior
{
	Depth32Behavior(Common& common, bool& v)
	: BooleanSwitchBehavior(common, v)
	{
	}
	
	int onEnter(Menu& menu, int item)
	{
		BooleanSwitchBehavior::onEnter(menu, item);
		gfx.setVideoMode();
		return -1;
	}
	
	bool onLeftRight(Menu& menu, int item, int dir)
	{
		BooleanSwitchBehavior::onLeftRight(menu, item, dir);
		gfx.setVideoMode();
		return true;
	}
};



void Gfx::updateExtensions(bool enabled)
{
	for(std::size_t i = HiddenMenu::Extensions + 1; i < HiddenMenu::FullscreenW; ++i)
	{
		hiddenMenu.setVisibility(i, enabled);
	}
	
	for(std::size_t i = 13; i < playerMenu.items.size(); ++i)
	{
		playerMenu.setVisibility(i, enabled);
	}
}

static std::string const scaleFilterNames[Settings::SfMax] =
{
	"Nearest",
	"Scale2X"
};


ItemBehavior* HiddenMenu::getItemBehavior(Common& common, int item)
{
	switch(item)
	{
		case Extensions:
			return new ExtensionsSwitchBehavior(common, gfx.settings->extensions);
		case RecordReplays:
			return new BooleanSwitchBehavior(common, gfx.settings->recordReplays);
		case Replays:
			return new ReplaySelectBehavior();
		case LoadPowerLevels:
			return new BooleanSwitchBehavior(common, gfx.settings->loadPowerlevelPalette);
		case ScalingFilter:
			return new ArrayEnumBehavior(common, gfx.settings->scaleFilter, scaleFilterNames);
		case FullscreenW:
			return new IntegerBehavior(common, gfx.settings->fullscreenW, 0, 9999, 0);
		case FullscreenH:
			return new IntegerBehavior(common, gfx.settings->fullscreenH, 0, 9999, 0);
		case Depth32:
			return new Depth32Behavior(common, gfx.settings->depth32);
		default:
			return Menu::getItemBehavior(common, item);
	}
}

struct LevelSort
{
	typedef std::pair<std::string, std::string> type;

	bool operator()(type const& a, type const& b) const
	{
		return ciLess(a.first, b.first);
	}
};

void Gfx::selectLevel()
{
	Menu levelMenu(178, 28);
	
	levelMenu.setHeight(14);
	
	levelMenu.addItem(MenuItem(48, 7, common->texts.random));
	
	bool altName = settings->extensions ? false : true;
	
	DirectoryIterator di(joinPath(lieroEXERoot, ".")); // TODO: Fix lieroEXERoot to be "." instead of ""
	
	std::vector<std::pair<std::string, std::string> > levels;
	
	for(; di; ++di)
	{
		std::string const& name = *di;
		std::string const& altName = di.alt();
		
		if(ciCompare(getExtension(name), "LEV"))
		{
			levels.push_back(std::make_pair(getBasename(name), getBasename(altName)));
			//levelMenu.addItem(MenuItem(7, 7, getBasename(str)));
		}
	}
	
	std::sort(levels.begin(), levels.end(), LevelSort());
	
	for(std::size_t i = 0; i < levels.size(); ++i)
	{
		if(altName)
			levelMenu.addItem(MenuItem(48, 7, levels[i].second));
		else
			levelMenu.addItem(MenuItem(48, 7, levels[i].first));
	}
	
	levelMenu.moveToFirstVisible();
	
	if(!settings->levelFile.empty())
	{
		for(std::size_t i = 1; i < levelMenu.items.size(); ++i)
		{
			if(ciCompare(levels[i-1].second, settings->levelFile))
			{
				levelMenu.moveTo(i);
				break;
			}
		}
	}


	do
	{
		std::memcpy(gfx.screenPixels, &frozenScreen[0], frozenScreen.size());
		
		drawBasicMenu();
		
		drawRoundedBox(178, 20, 0, 7, common->font.getDims(common->texts.selLevel));
		common->font.drawText(common->texts.selLevel, 180, 21, 50);

		levelMenu.draw(*common, false);
		
		if(testSDLKeyOnce(SDLK_UP))
		{
			sfx.play(26);
			
			levelMenu.movement(-1);
		}
		
		if(testSDLKeyOnce(SDLK_DOWN))
		{
			sfx.play(25);
			
			levelMenu.movement(1);
		}
		
		if(testSDLKeyOnce(SDLK_RETURN)
		|| testSDLKeyOnce(SDLK_KP_ENTER))
		{
			sfx.play(27);
			
			if(levelMenu.selection() == 0)
			{
				settings->randomLevel = true;
				settings->levelFile.clear();
			}
			else
			{
				settings->randomLevel = false;
				settings->levelFile = levels[levelMenu.selection() - 1].second;
			}
			
			break;
		}
		
		if(settings->extensions)
		{
			if(testSDLKeyOnce(SDLK_PAGEUP))
			{
				sfx.play(26);
				
				levelMenu.movementPage(-1);
			}
			
			if(testSDLKeyOnce(SDLK_PAGEDOWN))
			{
				sfx.play(25);
				
				levelMenu.movementPage(1);
			}
		}
		
		origpal.rotate(168, 174);
		pal = origpal;
		
		flip();
		process();
	}
	while(!testSDLKeyOnce(SDLK_ESCAPE));
}

void Gfx::selectProfile(WormSettings& ws)
{
	Menu profileMenu(28, 28);
	
	profileMenu.setHeight(14);
		
	DirectoryIterator di(joinPath(lieroEXERoot, ".")); // TODO: Fix lieroEXERoot to be "." instead of ""
	
	std::vector<std::string> profiles;
	
	for(; di; ++di)
	{
		std::string str = *di;
		
		if(ciCompare(getExtension(str), "LPF"))
			profiles.push_back(getBasename(str));
			
	}
	
	std::sort(profiles.begin(), profiles.end(), ciLess);
	
	for(std::size_t i = 0; i < profiles.size(); ++i)
		profileMenu.addItem(MenuItem(7, 7, profiles[i]));
	
	profileMenu.moveToFirstVisible();
	
	do
	{
		std::memcpy(gfx.screenPixels, &frozenScreen[0], frozenScreen.size());

		common->font.drawFramedText("Select profile", 28, 20, 50);

		profileMenu.draw(*common, false);
		
		if(testSDLKeyOnce(SDLK_UP))
		{
			sfx.play(26);
			
			profileMenu.movement(-1);
		}
		
		if(testSDLKeyOnce(SDLK_DOWN))
		{
			sfx.play(25);
			
			profileMenu.movement(1);
		}
		
		if(testSDLKeyOnce(SDLK_RETURN)
		|| testSDLKeyOnce(SDLK_KP_ENTER))
		{
			if(profileMenu.isSelectionValid())
			{
				ws.loadProfile(profileMenu.items[profileMenu.selection()].string);
				
				return;
			}
		}
		
		if(settings->extensions)
		{
			if(testSDLKeyOnce(SDLK_PAGEUP))
			{
				sfx.play(26);
				
				profileMenu.movementPage(-1);
			}
			
			if(testSDLKeyOnce(SDLK_PAGEDOWN))
			{
				sfx.play(25);
				
				profileMenu.movementPage(1);
			}
		}
		
		origpal.rotate(168, 174);
		pal = origpal;
		
		flip();
		process();
	}
	while(!testSDLKeyOnce(SDLK_ESCAPE));
	
	return;
}



int Gfx::selectReplay()
{
	Menu replayMenu(28, 28);
	
	replayMenu.setHeight(14);
		
	DirectoryIterator di(joinPath(lieroEXERoot, ".")); // TODO: Fix lieroEXERoot to be "." instead of ""
	
	std::vector<std::string> replays;
	
	for(; di; ++di)
	{
		std::string str = *di;
		
		if(ciCompare(getExtension(str), "LRP"))
			replays.push_back(getBasename(str));
	}
	
	std::sort(replays.begin(), replays.end(), ciLess);
	
	for(std::size_t i = 0; i < replays.size(); ++i)
		replayMenu.addItem(MenuItem(7, 7, replays[i]));
	
	replayMenu.moveToFirstVisible();
	
	do
	{
		std::memcpy(gfx.screenPixels, &frozenScreen[0], frozenScreen.size());
		
		std::string selReplay = "Select replay";
		
		common->font.drawFramedText(selReplay, 28, 20, 50);
		/*
		drawRoundedBox(28, 20, 0, 7, common->font.getDims(selReplay));
		common->font.drawText(selReplay, 30, 21, 50);*/

		replayMenu.draw(*common, false);
		
		if(testSDLKeyOnce(SDLK_UP))
		{
			sfx.play(26);
			
			replayMenu.movement(-1);
		}
		
		if(testSDLKeyOnce(SDLK_DOWN))
		{
			sfx.play(25);
			
			replayMenu.movement(1);
		}
		
		if(testSDLKeyOnce(SDLK_RETURN)
		|| testSDLKeyOnce(SDLK_KP_ENTER))
		{
			if(replayMenu.isSelectionValid())
			{
				std::string replayName = replayMenu.items[replayMenu.selection()].string + ".lrp";			
				std::string fullPath = joinPath(lieroEXERoot, replayName);
				
				// Reset controller before opening the replay, since we may be recording it
				controller.reset();
				
				gvl::stream_ptr replay(new gvl::fstream(std::fopen(fullPath.c_str(), "rb")));
				controller.reset(new ReplayController(common, replay));
				
				return MaReplay;
			}
		}
		
		if(settings->extensions)
		{
			if(testSDLKeyOnce(SDLK_PAGEUP))
			{
				sfx.play(26);
				
				replayMenu.movementPage(-1);
			}
			
			if(testSDLKeyOnce(SDLK_PAGEDOWN))
			{
				sfx.play(25);
				
				replayMenu.movementPage(1);
			}
		}
		
		origpal.rotate(168, 174);
		pal = origpal;
		
		flip();
		process();
	}
	while(!testSDLKeyOnce(SDLK_ESCAPE));
	
	return -1;
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
		std::memcpy(gfx.screenPixels, &frozenScreen[0], frozenScreen.size());
		
		drawBasicMenu();
		
		drawRoundedBox(179, 20, 0, 7, common->font.getDims(common->texts.weapon));
		drawRoundedBox(249, 20, 0, 7, common->font.getDims(common->texts.availability));
		
		common->font.drawText(common->texts.weapon, 181, 21, 50);
		common->font.drawText(common->texts.availability, 251, 21, 50);
		
		weaponMenu.draw(*common, false);
						
		if(testSDLKeyOnce(SDLK_UP))
		{
			sfx.play(26);
			weaponMenu.movement(-1);
		}
		
		if(testSDLKeyOnce(SDLK_DOWN))
		{
			sfx.play(25);
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
				sfx.play(26);
				
				weaponMenu.movementPage(-1);
			}
			
			if(testSDLKeyOnce(SDLK_PAGEDOWN))
			{
				sfx.play(25);
				
				weaponMenu.movementPage(1);
			}
		}

		origpal.rotate(168, 174);
		pal = origpal;
		
		flip();
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
		SDL_FillRect(screen, 0, bgColor);
	}
	
	int height;
	int width = common->font.getDims(text, &height);
	
	int cx = x - width/2 - 2;
	int cy = y - height/2 - 2;
	
	drawRoundedBox(cx, cy, 0, height+1, width+1);
	common->font.drawText(text, cx+2, cy+2, 6);
	
	flip();
	process();
	
	waitForKey();
	clearKeys();
	
	if(clearScreen)
		SDL_FillRect(screen, 0, bgColor);
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
		
		blitImageNoKeyColour(screen, &frozenScreen[offset], clrX, y, clrX + 10 + width, 8, 320);
		
		drawRoundedBox(x - 2 - adjust, y, 0, 7, width);
		
		font.drawText(str, x - adjust, y + 1, 50);
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
			sfx.play(27);
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
			drawRoundedBox(x + 24, y, 168, 7, ws->rgb[rgbcol] - 1);
		}
		else // CE98
		{
			drawRoundedBox(x + 24, y, 0, 7, ws->rgb[rgbcol] - 1);
		}
		
		fillRect(x + 25, y + 1, ws->rgb[rgbcol], 5, ws->colour);
	} // CED9
}



ItemBehavior* PlayerMenu::getItemBehavior(Common& common, int item)
{
	switch(item)
	{
		case 0:
			return new WormNameBehavior(common, *ws);
		case 1:
			return new IntegerBehavior(common, ws->health, 1, 10000, 1, true);
		case 2:
		case 3:
		case 4:
			return new IntegerBehavior(common, ws->rgb[item - 2], 0, 63, 1, false);
			
		case 5: // D2AB
		case 6:
		case 7:
		case 8:
		case 9:
		case 10:
		case 11:
			return new KeyBehavior(common, ws->controls[item - 5]);
			
		case 12: // Controller
		{
			// Controller cannot be changed with Enter
			return new ArrayEnumBehavior(common, ws->controller, common.texts.controllers, true);
		}
		
		case 13: // Save profile
			return new ProfileSaveBehavior(common, *ws, false);
			
		case 14: // Save profile as
			return new ProfileSaveBehavior(common, *ws, true);
			
		case 15:
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
#if 0
	//playerMenuValues.selection = playerMenu.selection;
	
	do
	{
		//int selectY = (playerMenu.selection() << 3) + 20;
		
		drawBasicMenu();

		playerMenu.draw(*common, false);
		//playerMenuValues.draw(*common, 273, 20, false);
		
		drawRoundedBox(163, 19, 0, 12, 11);

		blitImage(gfx.screen, common->wormSprite(2, 1, player), 163, 20, 16, 16);
		

		// l_CF9E:

		if(testSDLKeyOnce(SDLK_UP))
		{
			sfx.play(26);
			
			playerMenu.movement(-1);
		} // CFD0

		if(testSDLKeyOnce(SDLK_DOWN))
		{
			sfx.play(25);

			playerMenu.movement(1);
		} // D002
		
		
		if(testSDLKey(SDLK_LEFT))
		{
			if(!playerMenu.onLeftRight(*common, -1))
				resetLeftRight();
		}
		if(testSDLKey(SDLK_RIGHT))
		{
			if(!playerMenu.onLeftRight(*common, 1))
				resetLeftRight();
		}
		
		if(testSDLKeyOnce(SDLK_RETURN)
		|| testSDLKeyOnce(SDLK_KP_ENTER))
		{
			playerMenu.onEnter(*common);
		}
		
		origpal.setWormColours(*settings);
		origpal.rotate(168, 174);
		pal = origpal;
 
		flip();
		process();
		
		menuCyclic = (menuCyclic + 1) & 3;
	}
	while(!testSDLKeyOnce(SDLK_ESCAPE));
#endif

}

/*
void Gfx::settingLeftRight(int change, int item)
{
	if(!settingsMenu.onLeftRight(*common, change)) // TODO: This assumes the item is selected, we should make settingLeftRight do so too
	{
		resetLeftRight();
	}
}*/

void Gfx::mainLoop()
{
	Rand rand = gfx.rand;
	controller.reset(new LocalController(common, settings));
	
	{
		Level newLevel;
		newLevel.generateFromSettings(*common, *settings, rand);
		controller->swapLevel(newLevel);
	}
	
	controller->currentGame()->focus();
	// TODO: Unfocus game when necessary
	
	while(true)
	{
		clear();
		controller->draw();
		
		gfx.mainMenu.setVisibility(0, controller->running());
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
				Level newLevel;
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
			controller->draw();
			
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

void Gfx::saveSettings()
{
	settings->save(joinPath(lieroEXERoot, settingsFile + ".DAT"));
}

bool Gfx::loadSettings()
{
	settings.reset(new Settings);
	return settings->load(joinPath(lieroEXERoot, settingsFile + ".DAT"));
}

void Gfx::drawBasicMenu(/*int curSel*/)
{
	std::memcpy(screen->pixels, &frozenScreen[0], frozenScreen.size());
	
	common->font.drawText(common->texts.saveoptions, 36, 54+20, 0);
	common->font.drawText(common->texts.loadoptions, 36, 61+20, 0);
	
	common->font.drawText(common->texts.saveoptions, 36, 53+20, 10);
	common->font.drawText(common->texts.loadoptions, 36, 60+20, 10);
	
	if(settingsFile.empty())
	{
		common->font.drawText(common->texts.curOptNoFile, 36, 46+20, 0);
		common->font.drawText(common->texts.curOptNoFile, 35, 45+20, 147);
	}
	else
	{
		common->font.drawText(common->texts.curOpt + settingsFile, 36, 46+20, 0);
		common->font.drawText(common->texts.curOpt + settingsFile, 35, 45+20, 147);
	}
	
	
/* TODO
	if(!settingsfile[0])
	{
		DrawTextMW(txt_curoptnofile, 36, 46+20, 0);
		DrawTextMW(txt_curoptnofile, 35, 45+20, 147);
	} else // E8A6
	{
char buffer[256];
		sprintf(buffer, txt_curopt, settingsfile);
		DrawTextMW(buffer, 36, 46+20, 0);
		DrawTextMW(buffer, 35, 45+20, 147);
	} // E90E
	*/
	
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

int Gfx::menuLoop()
{
	std::memset(pal.entries, 0, sizeof(pal.entries));
	flip();
	process();
	
	fillRect(0, 151, 160, 7, 0);
	common->font.drawText(common->texts.copyright2, 2, 152, 19);
	
	mainMenu.moveToFirstVisible();
	settingsMenu.moveToFirstVisible();
	settingsMenu.updateItems(*common);
	
	fadeValue = 0;
	curMenu = &mainMenu;

	std::memcpy(&frozenScreen[0], screen->pixels, frozenScreen.size());

	updateExtensions(settings->extensions);
	updateSettingsMenu();
	
	menuCyclic = 0;
	int selected = -1;
		
	do
	{
		if(curMenu == &playerMenu)
			menuCyclic = (menuCyclic + 1) & 3;
		else
			menuCyclic = (menuCyclic + 1) % 5;
		
		drawBasicMenu();
		
		if(curMenu == &mainMenu)
			settingsMenu.draw(*common, true);
		else
			curMenu->draw(*common, false);
		
		if(testSDLKeyOnce(SDLK_ESCAPE))
		{
			if(curMenu == &playerMenu)
				curMenu = &settingsMenu;
			else if(curMenu == &mainMenu)
				mainMenu.moveTo(3);
			else
				curMenu = &mainMenu;
		}
		
		if(testSDLKeyOnce(SDLK_UP))
		{
			sfx.play(26);
			
			curMenu->movement(-1);
		}
		
		if(testSDLKeyOnce(SDLK_DOWN))
		{
			sfx.play(25);
			
			curMenu->movement(1);
		}

		if(testSDLKeyOnce(SDLK_RETURN)
		|| testSDLKeyOnce(SDLK_KP_ENTER))
		{
			if(curMenu == &mainMenu)
			{
				sfx.play(27);
				
				if(mainMenu.selection() == MaSettings)
				{
					curMenu = &settingsMenu; // Go into settings menu
				}
				else // ED71
				{
					curMenu = &mainMenu;
					selected = mainMenu.selection();
				} // ED75
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
		
		if(testSDLKeyOnce(SDLK_F1)
		&& curMenu != &hiddenMenu)
		{
			curMenu = &hiddenMenu;
			curMenu->updateItems(*common);
			curMenu->moveToFirstVisible();
		}
		
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
		
		if(settings->extensions)
		{
			if(testSDLKeyOnce(SDLK_PAGEUP))
			{
				sfx.play(26);
				
				curMenu->movementPage(-1);
			}
			
			if(testSDLKeyOnce(SDLK_PAGEDOWN))
			{
				sfx.play(25);
				
				curMenu->movementPage(1);
			}
		}

		origpal.setWormColours(*settings);
		origpal.rotate(168, 174);
		pal = origpal;

		if(fadeValue < 32)
		{
			fadeValue += 1;
			pal.fade(fadeValue);
		} // EDE3
		
		flip();
		process();
	}
	while(selected < 0);

	for(fadeValue = 32; fadeValue > 0; --fadeValue)
	{
		pal = origpal;
		pal.fade(fadeValue);
		flip(); // TODO: We should just screen sync and set the palette here
	} // EE36
	
	return selected;
}


