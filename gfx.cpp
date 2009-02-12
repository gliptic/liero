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
#include <SDL/SDL.h>
//#include <iostream>
#include <cstdio>
#include <memory>
#include "controller.hpp"

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
	
	fread(&temp[0], 1, amount, f);
	
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

Gfx::Gfx()
: firstMenuItem(1)
, settingsMenuValues(true)
, playerMenuValues(true)
, screen(0)
, back(0)
, frozenScreen(320 * 200)
, running(true)
, fullscreen(false)
, doubleRes(false)
, menuCyclic(0)
, fadeValue(0)
{
	clearKeys();
}

void Gfx::init()
{
	setVideoMode();
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
	SDL_EnableUNICODE(1);
	SDL_WM_SetCaption("Liero", 0);
	SDL_ShowCursor(SDL_DISABLE);
	lastFrame = SDL_GetTicks();
}

void Gfx::setVideoMode()
{
	int flags = SDL_SWSURFACE;
	if(fullscreen)
		flags |= SDL_FULLSCREEN;
		
	if(screen != back)
	{
		SDL_FreeSurface(screen);
		screen = 0;
	}

	if(!SDL_VideoModeOK(320, 200, 8, flags)
	|| gfx.doubleRes)
	{
		screen = SDL_CreateRGBSurface(SDL_SWSURFACE, 320, 200, 8, 0, 0, 0, 0);
		back = SDL_SetVideoMode(640, 480, 8, flags);
	}
	else
	{
		back = screen = SDL_SetVideoMode(320, 200, 8, flags);
	}
	screenPixels = static_cast<unsigned char*>(screen->pixels);
	screenPitch = screen->pitch;
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
	
	settingsMenuValues.items.assign(12, MenuItem(48, 7, ""));
	
	fseek(exe, 0x1B210, SEEK_SET);
	playerMenu.readItems(exe, 13, 13, false, 48, 7);
	
	playerMenuValues.items.assign(13, MenuItem(48, 7, ""));
}




void Gfx::updateSettingsMenu()
{
	settingsMenuValues.items[0].string = common->texts.gameModes[settings->gameMode];
	
	switch(settings->gameMode)
	{
		case Settings::GMKillEmAll:
			settingsMenuValues.items[1].string = toString(settings->lives);
			settingsMenu.items[1].string = common->texts.gameModeSpec[0];
		break;
		
		case Settings::GMGameOfTag:		
			settingsMenuValues.items[1].string = timeToString(settings->timeToLose);
			settingsMenu.items[1].string = common->texts.gameModeSpec[1];
		break;
		
		case Settings::GMCtF:
		case Settings::GMSimpleCtF:
			settingsMenuValues.items[1].string = toString(settings->flagsToWin);
			settingsMenu.items[1].string = common->texts.gameModeSpec[2];
		break;
	}
	
	settingsMenuValues.items[2].string = toString(settings->loadingTime) + '%';
	settingsMenuValues.items[3].string = toString(settings->maxBonuses);
	
	settingsMenuValues.items[4].string = common->texts.onoff[settings->namesOnBonuses];
	settingsMenuValues.items[5].string = common->texts.onoff[settings->map];
	
	settingsMenuValues.items[6].string = toString(settings->blood) + '%';
	
	std::string levelPath = joinPath(lieroEXERoot, settings->levelFile + ".lev");
	if(!settings->randomLevel && fileExists(levelPath))
	{
		settingsMenuValues.items[7].string = '"' + settings->levelFile + '"';
		settingsMenuValues.items[8].string = common->texts.reloadLevel;
	}
	else
	{
		settingsMenuValues.items[7].string = common->texts.random2;
		settingsMenuValues.items[8].string = common->texts.regenLevel;
	}
	
	settingsMenuValues.items[8].string = common->texts.onoff[settings->regenerateLevel];
	settingsMenuValues.items[9].string = common->texts.onoff[settings->shadow];
	settingsMenuValues.items[10].string = common->texts.onoff[settings->screenSync];
	settingsMenuValues.items[11].string = common->texts.onoff[settings->loadChange];
	
}

void Gfx::updatePlayerMenu(int player)
{
	WormSettings const& ws = *settings->wormSettings[player];
	
	playerMenuValues.items[0].string = ws.name;
	playerMenuValues.items[1].string = toString(ws.health) + '%';
	playerMenuValues.items[2].string = toString(ws.rgb[0]);
	playerMenuValues.items[3].string = toString(ws.rgb[1]);
	playerMenuValues.items[4].string = toString(ws.rgb[2]);
	
	for(int i = 0; i < 7; ++i)
	{
		playerMenuValues.items[i + 5].string = common->texts.keyNames[ws.controls[i]];
	}

	playerMenuValues.items[12].string = common->texts.controllers[ws.controller];
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
				setVideoMode();
			}
			else if(s == SDLK_F6)
			{
				doubleRes = !doubleRes;
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

void Gfx::flip()
{
	pal.activate();
	if(screen != back)
	{
		PalIdx* dest = reinterpret_cast<PalIdx*>(back->pixels);
		PalIdx* src = screenPixels;
		
		std::size_t destPitch = back->pitch;
		std::size_t srcPitch = screenPitch;
		
		for(int y = 0; y < 200; ++y)
		for(int x = 0; x < 320; ++x)
		{
			int dx = (x << 1);
			int dy = (y << 1) + 40;
			PalIdx pix = src[y*srcPitch + x];
			dest[dy*destPitch + dx] = pix;
			dest[dy*destPitch + (dx+1)] = pix;
			dest[(dy+1)*destPitch + dx] = pix;
			dest[(dy+1)*destPitch + (dx+1)] = pix;
		}
	}
	
	SDL_Flip(back);
	
	if(settings->screenSync)
	{
		static unsigned int const delay = 14u;
		
		while((SDL_GetTicks() - lastFrame) < delay)
		{
			SDL_Delay(0);
		}
		
		while((SDL_GetTicks() - lastFrame) >= delay)
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
		sfx.play(25, -1);
	}
	else
	{
		sfx.play(26, -1);
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

void Gfx::settingEnter(int item)
{
	int selectY = item * 8 + 20;
	switch(item)
	{
		case 0: //GAME MODE
			settings->gameMode = (settings->gameMode + 1) % 4;
		break;
		
		case 1:  //LIVES / TIME TO LOSE / FLAGS TO WIN //D772
			switch(settings->gameMode)
			{
				case Settings::GMKillEmAll:
					inputInteger(settings->lives, 0, 999, 3, 280, selectY);
				break;
				
				case Settings::GMCtF: // D7AF
				case Settings::GMSimpleCtF:
					inputInteger(settings->flagsToWin, 0, 999, 3, 280, selectY);
				break;
			} // D7E7
		break;
			
		case 2: // LOADING TIMES // D7EA
			inputInteger(settings->loadingTime, 0, 9999, 4, 280, selectY);
		break;
		
		case 3: // MAX BONUSES // D82A
			inputInteger(settings->maxBonuses, 0, 99, 2, 280, selectY);
   		break;
   		
		case 4: // NAMES ON BONUSES // D86B
			settings->namesOnBonuses ^= 1; //Toggles first bit
		break;
		
		case 5: // MAP //D87F
			settings->map ^= 1;
		break;
		
		case 7: // LEVEL //D893
		{
			std::vector<std::string> list;
			
			list.push_back(common->texts.random);
			
			DirectoryIterator di(joinPath(lieroEXERoot, ".")); // TODO: Fix lieroEXERoot to be "." instead of ""
			
			for(; di; ++di)
			{
				std::string str = *di;
				
				if(ciCompare(getExtension(str), "LEV"))
					list.push_back(getBasename(str));
			}
			
			std::size_t curSel = 0;
			std::size_t topItem = 0;
			
			if(!settings->levelFile.empty())
			{
				for(std::size_t i = 1; i < list.size(); ++i)
				{
					if(ciCompare(list[i], settings->levelFile))
					{
						curSel = i;
						break;
					}
				}
			}
			
			if(list.size() > curSel + 14) // We subtract 14 instead of 13, since list.size() seems to be 1 too much compared to 'listitems' in the original
				topItem = curSel;
			else if(list.size() >= 14)
				topItem = list.size() - 14;
			else
				topItem = 0;
				
			std::vector<PalIdx> tempScreen(320 * 200);
			
			// Reset the right part of the screen
			blitImageNoKeyColour(screen, &frozenScreen[160], 160, 0, 160, 200, 320);
			
			drawRoundedBox(178, 20, 0, 7, common->font.getWidth(common->texts.selLevel));
			common->font.drawText(common->texts.selLevel, 180, 21, 50);
			
			std::memcpy(&tempScreen[0], gfx.screenPixels, tempScreen.size());
			
			do
			{
				std::memcpy(gfx.screenPixels, &tempScreen[0], tempScreen.size());
								
				std::size_t max = topItem + 14;
				
				for(std::size_t i = topItem; i < max; ++i)
				{
					if(i < list.size())
					{
						int y = int(i - topItem) * 8 + 29;
						std::string item = list[i];
						toUpperCase(item); // TODO: Maybe optimize this
						
						if(i == curSel)
						{
							drawRoundedBox(178, int(curSel - topItem) * 8 + 28, 0, 7, common->font.getWidth(item));
						}
						
						common->font.drawText(item, 181, y + 1, 0);
						common->font.drawText(item, 180, y, (i != curSel) ? 7 : 168);
					}
				}
				
				if(list.size() > 14)
				{
					common->font.drawChar(22, 172, 30, 0);
					common->font.drawChar(22, 171, 29, 50);
					common->font.drawChar(23, 172, 134, 0);
					common->font.drawChar(23, 171, 133, 50);
					
					int height = int(14*96 / list.size());
					int y = int(topItem * 96 / list.size());
					
					fillRect(171, y + 37, 7, height, 0);
					fillRect(170, y + 36, 7, height, 7);
				}
				
				if(testSDLKeyOnce(SDLK_UP))
				{
					sfx.play(26, -1);
					
					if(curSel > 0)
						--curSel;
					if(topItem > curSel)
						topItem = curSel;
				}
				
				if(testSDLKeyOnce(SDLK_DOWN))
				{
					sfx.play(25, -1);
					
					if(curSel < list.size() - 1)
						++curSel;
					if(topItem + 13 < curSel)
						topItem = curSel - 13;
				}
				
				if(testSDLKeyOnce(SDLK_RETURN)
				|| testSDLKeyOnce(SDLK_KP_ENTER))
				{
					sfx.play(27, -1);
					
					if(curSel == 0)
					{
						settings->randomLevel = true;
						settings->levelFile.clear();
					}
					else
					{
						settings->randomLevel = false;
						settings->levelFile = list[curSel];
					}
					
					break;
				}
				
				origpal.rotate(168, 174);
				pal = origpal;
				
				flip();
				process();
			}
			while(!testSDLKeyOnce(SDLK_ESCAPE));
		}
		break;
		
		case 8: // REGENERATE LEVEL // DD92
			settings->regenerateLevel ^= 1;
		break;
		case 9: // SHADOWS // DDA6
			settings->shadow ^= 1;
		break;
		case 10: // SCREEN SYNC // DDBA
			settings->screenSync ^= 1;
		break;
		case 11: // LOAD+CHANGE // DDCE
			settings->loadChange ^= 1;
		break;
		
		case 12: // PLAYER 1 OPTIONS // DDE2
			playerSettings(0);
		break;
		case 13: // PLAYER 2 OPTIONS // DDF2
			playerSettings(1);
		break;
		case 14: // WEAPON OPTIONS // DE02
		{
			std::size_t curSel = 1;
			std::size_t topItem = 1;
			std::size_t listItems = 40;
			
			std::vector<PalIdx> tempScreen(320 * 200);
			
			// Reset the right part of the screen
			blitImageNoKeyColour(screen, &frozenScreen[160], 160, 0, 160, 200, 320);
			
			drawRoundedBox(178, 20, 0, 7, common->font.getWidth(common->texts.weapon));
			drawRoundedBox(248, 20, 0, 7, common->font.getWidth(common->texts.availability));
			
			common->font.drawText(common->texts.weapon, 180, 21, 50);
			common->font.drawText(common->texts.availability, 250, 21, 50);
			
			std::memcpy(&tempScreen[0], gfx.screenPixels, tempScreen.size());
			
			while(true)
			{
				std::memcpy(gfx.screenPixels, &tempScreen[0], tempScreen.size());
				
				std::size_t max = topItem + 14;
				for(std::size_t i = topItem; i < max; ++i)
				{
					if(i <= listItems)
					{
						int index = common->weapOrder[i];
						int state = settings->weapTable[index];
						std::string const& stateStr = common->texts.weapStates[state];
						std::string const& weapName = common->weapons[index].name;
						int nameWidth = common->font.getWidth(weapName);
						int stateWidth = common->font.getWidth(stateStr);
						
						int y = int(i - topItem) * 8 + 29;
						
						if(i == curSel)
						{
							drawRoundedBox(178, y - 1, 0, 7, nameWidth);
							drawRoundedBox(268 - stateWidth/2, y - 1, 0, 7, stateWidth);
						}
						
						common->font.drawText(weapName, 181, y + 1, 0); // TODO: A single function drawing text with shadow
						common->font.drawText(weapName, 180, y, (i != curSel) ? 7 : 168);
						common->font.drawText(stateStr, 271 - stateWidth/2, y + 1, 0);
						common->font.drawText(stateStr, 270 - stateWidth/2, y, (i != curSel) ? 7 : 168);
						
					}
				}
				
				
				common->font.drawChar(22, 172, 30, 0);
				common->font.drawChar(22, 171, 29, 50);
				common->font.drawChar(23, 172, 134, 0);
				common->font.drawChar(23, 171, 133, 50);
				
				int height = 33;
				int y = int((topItem * 96 + 96) / 40);
				
				fillRect(171, y + 34, 7, height, 0);
				fillRect(170, y + 33, 7, height, 7);
				
				if(testSDLKeyOnce(SDLK_UP))
				{
					sfx.play(26, -1);
					
					if(curSel > 1)
						--curSel;
					if(topItem > curSel)
						topItem = curSel;
				}
				
				if(testSDLKeyOnce(SDLK_DOWN))
				{
					sfx.play(25, -1);
					
					if(curSel < 40)
						++curSel;
					if(topItem + 13 < curSel)
						++topItem;
				}
				
				if(testSDLKeyOnce(SDLK_LEFT))
				{
					sfx.play(25, -1);
					
					unsigned char& v = settings->weapTable[common->weapOrder[curSel]];
					
					v = (v - 1 + 3) % 3;
				}
				
				if(testSDLKeyOnce(SDLK_RIGHT))
				{
					sfx.play(26, -1);
					
					unsigned char& v = settings->weapTable[common->weapOrder[curSel]];
					
					v = (v + 1 + 3) % 3;
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
						
					drawRoundedBox(178, 58, 0, 17, 98);
					common->font.drawText(common->texts.noWeaps, 180, 60, 6);
					
					flip();
					process();
					
					gfx.waitForKey();
				}
			}
			
			sfx.play(27, -1);
		}
		break;
	}
}

bool Gfx::inputString(std::string& dest, std::size_t maxLen, int x, int y, int (*filter)(int), std::string const& prefix, bool centered)
{
	std::string buffer = dest;
	
	while(true)
	{
		std::string str = prefix + buffer + '_';
		
		Font& font = common->font;
		
		int width = font.getWidth(str);
		
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
			sfx.play(27, -1);
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


void Gfx::playerSettings(int player)
{
	int curSel = 0;
	int menuCyclic = 0;
	
	WormSettings& ws = *settings->wormSettings[player];
	
	updatePlayerMenu(player);
	
	do
	{
		int selectY = (curSel << 3) + 20;
		
		drawBasicMenu(0);

		playerMenu.draw(*common, 178, 20, false, curSel);
		playerMenuValues.draw(*common, 273, 20, false, curSel);
		
		for(int o = 0; o < 12; o++)
		{
			int ypos = (o<<3);

			if(o >= 2 && o <= 4) //Color settings
			{
				int rgbcol = o - 2;

				if(o == curSel)
				{
					drawRoundedBox(202, ypos + 20, 168, 7, ws.rgb[rgbcol] - 1);
				}
				else // CE98
				{
					drawRoundedBox(202, ypos + 20, 0, 7, ws.rgb[rgbcol] - 1);
				}
				
				fillRect(203, ypos + 21, ws.rgb[rgbcol], 5, ws.colour);
			} // CED9
			
			
		} // CF22
		
		drawRoundedBox(163, 19, 0, 12, 11);

		blitImage(gfx.screen, common->wormSprite(2, 1, player), 163, 20, 16, 16);
		

		// l_CF9E:

		if(testSDLKeyOnce(SDLK_UP))
		{
			sfx.play(26, -1);
			--curSel;
			if(curSel < 0)
				curSel = 12;
		} // CFD0

		if(testSDLKeyOnce(SDLK_DOWN))
		{
			sfx.play(25, -1);
		
			++curSel;
			if(curSel > 12)
				curSel = 0;
		} // D002
		
		if(menuCyclic == 0)
		{
		
			switch(curSel)
			{
			case 1:
				
				if(testSDLKey(SDLK_LEFT))
				{
					if(ws.health > 1)
						--ws.health;
					updatePlayerMenu(player);
				}
				if(testSDLKey(SDLK_RIGHT))
				{
					if(ws.health < 10000)
						++ws.health;
					updatePlayerMenu(player);
				}
			
			break;
			
			case 2:
			case 3:
			case 4:
				
				if(testSDLKey(SDLK_LEFT))
				{
					--ws.rgb[curSel - 2];
					if(ws.rgb[curSel - 2] < 0)
						ws.rgb[curSel - 2] = 0;
					updatePlayerMenu(player);
				}
				if(testSDLKey(SDLK_RIGHT))
				{
					++ws.rgb[curSel - 2];
					if(ws.rgb[curSel - 2] > 63)
						ws.rgb[curSel - 2] = 63;
					updatePlayerMenu(player);
				}
				
			break;
			
			case 12:
				
				if(testSDLKeyOnce(SDLK_LEFT))
				{
					sfx.play(25, -1); // Should it be 26?
					ws.controller = (ws.controller - 1 + 2) % 2;
					updatePlayerMenu(player);
				}
				if(testSDLKeyOnce(SDLK_RIGHT))
				{
					sfx.play(26, -1); // Should it be 25?
					ws.controller = (ws.controller + 1) % 2;
					updatePlayerMenu(player);
				}
			break;
			
			}
		}
		
		if(testSDLKeyOnce(SDLK_RETURN)
		|| testSDLKeyOnce(SDLK_KP_ENTER))
		{
			sfx.play(27, -1);
			
			switch(curSel)
			{
			case 0:
				ws.randomName = false;
				inputString(ws.name, 20, 275, 20);
				
				if(ws.name.empty())
				{
					Settings::generateName(ws);
				}
				//updatePlayerMenu(player);
			break;
			
			case 1:
				inputInteger(ws.health, 0, 10000, 5, 275, 28);
			break;
			
			case 2:
			case 3:
			case 4:
				inputInteger(ws.rgb[curSel-2], 0, 63, 2, 275, selectY);
			break;
			
			case 5: // D2AB
			case 6:
			case 7:
			case 8:
			case 9:
			case 10:
			case 11:
			{
				SDL_keysym key(waitForKey());
				
				if(key.sym != SDLK_ESCAPE)
				{
					Uint32 k = SDLToDOSKey(key.sym);
					if(k)
						ws.controls[curSel - 5] = k;
				/*
					int lieroKey = SDLToLieroKeys[key.sym];
					
					if(lieroKey)
					{
						ws.controls[curSel - 5] = lieroKey;
					}
				*/
				}
				
				clearKeys();
			}
			break;
			
			}
			
			updatePlayerMenu(player);
		}
		
		origpal.setWormColours(*settings);
		origpal.rotate(168, 174);
		pal = origpal;
 
		flip();
		process();
		
		menuCyclic = (menuCyclic + 1) & 3;
	}
	while(!testSDLKeyOnce(SDLK_ESCAPE));
	
}

void Gfx::settingLeftRight(int change, int item)
{
	switch(item)
	{
		case 4: //NAMES ON BONUSES
		case 5: //MAP
		case 8: //REGENERATE LEVEL
		case 9: //SHADOWS
		case 10: //SCREEN SYNC
		case 11: //LOAD+CHANGE
			playChangeSound(change);

			resetLeftRight();

			settingEnter(item);
		break;
			
		case 0: //GAME MODE // E4CF
			playChangeSound(change);

			resetLeftRight();

			settings->gameMode = (settings->gameMode + change + 4) % 4;
			
			
		break;
		
		case 1: //LIVES / TIME TO LOSE / FLAGS TO WIN //E530
			if(menuCyclic == 0)
			{
				switch(settings->gameMode)
				{
				case Settings::GMKillEmAll:
					changeVariable(settings->lives, change, 1, 999, 1);
					
				break;
				
				case Settings::GMGameOfTag: // E579
					changeVariable(settings->timeToLose, change, 60, 3600, 10);
				break;
				
				case Settings::GMCtF:
				case Settings::GMSimpleCtF: //E5B0
					changeVariable(settings->flagsToWin, change, 1, 999, 1);
				break;
				} // E5E3
			}
			break;
		case 2: //LOADING TIMES // E5E6
			if(menuCyclic == 0)
			{
				changeVariable(settings->loadingTime, change, 0, 9999, 1);
			}
			break;
		case 3: //MAX BONUSES // E622
			if(menuCyclic == 0)
			{
				changeVariable(settings->maxBonuses, change, 0, 99, 1);
			}
			break;
		case 6: //AMOUNT OF BLOOD // E65B
			if(menuCyclic == 0)
			{
				changeVariable(settings->blood, change, 0, 500, 25);
				
			}
			break;
	} // E68F
}

void Gfx::mainLoop()
{
#if 0
	std::auto_ptr<Game> game(new Game(common, settings));
	
	game->generateLevel();
	// Seems unnecessary: game->resetWorms();
	//game->enter();
	
	while(true)
	{
		// This doesn't seem to fill any useful purpose since we speparated out the drawing: game->processViewports();
		game->draw();
		
		int selection = menuLoop();
		
		if(selection == 1)
		{
			std::auto_ptr<Game> newGame(new Game(common, settings));
			
			if(!settings.regenerateLevel
			&& settings.randomLevel == game->oldRandomLevel
			&& settings.levelFile == game->oldLevelFile)
			{
				// Take level and palette from old game
				// TODO: These should be packaged together in a unit. Consider moving the palette to the level.
				newGame->level.swap(game->level);
				newGame->oldLevelFile = game->oldLevelFile;
				newGame->oldRandomLevel = game->oldRandomLevel;
			}
			else
			{
				newGame->generateLevel();
			}
			
			game = newGame;
			game->settings = settings; // Update game settings
			game->enter();
			game->startGame();
			//game->gameLoop();
		}
		else if(selection == 0)
		{
			game->settings = settings; // Update game settings
			game->enter();
			//game->continueGame();
			//game->gameLoop();
		}
		else if(selection == 3) // QUIT TO OS
		{
			break;
		}
		
		game->shutDown = false;
	
		do
		{
			game->processFrame();
			gfx.clear();
			game->draw();
			
			flip();
			process(game.get());
		}
		while(fadeValue > 0);
		
		clearKeys();
	}
#else
	Rand rand; // TODO: Seed
	std::auto_ptr<Controller> controller(new LocalController(common, settings));
	
	// Seems unnecessary: game->resetWorms();
	//game->enter();
	
	Level newLevel;
	newLevel.generateFromSettings(*common, *settings, rand);
	controller->swapLevel(newLevel);
	//controller->focus();
	
	while(true)
	{
		// This doesn't seem to fill any useful purpose since we separated out the drawing: game->processViewports();
		//game->draw();
		controller->draw();
		
		int selection = menuLoop();
		
		if(selection == 1)
		{
#if 0
			std::auto_ptr<Controller> newController(new LocalController(common, settings));
			
			Level& oldLevel = controller->currentLevel();
			
			if(!settings->regenerateLevel
			&& settings->randomLevel == oldLevel.oldRandomLevel
			&& settings->levelFile == oldLevel.oldLevelFile)
			{
				// Take level and palette from old game
				// TODO: These should be packaged together in a unit. Consider moving the palette to the level.
				newController->swapLevel(oldLevel);
			}
			else
			{
				Level newLevel;
				newLevel.generateFromSettings(*common, *settings, rand);
				newController->swapLevel(newLevel);
			}
			
			controller = newController;
#else
			controller.reset(new ReplayController(common, settings));
#endif
		}
		else if(selection == 0)
		{
			
		}
		else if(selection == 3) // QUIT TO OS
		{
			break;
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
		
		if(controller->running())
			firstMenuItem = 0;
		else
			firstMenuItem = 1;
		
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
#endif

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

void Gfx::drawBasicMenu(int curSel)
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
	
	if(curMenu == 0)
		mainMenu.draw(*common, 53, 20, false, curSel, firstMenuItem);
	else
		mainMenu.draw(*common, 53, 20, true, -1, firstMenuItem);
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
	
	int curSel = firstMenuItem;
	int curSel2 = 0;
	fadeValue = 0;
	curMenu = 0;

	std::memcpy(&frozenScreen[0], screen->pixels, frozenScreen.size());

	updateSettingsMenu();
	
	bool menuRunning = true;
	menuCyclic = 0;
		
	do
	{
		menuCyclic = (menuCyclic + 1) % 5;
		
		drawBasicMenu(curSel);
		
		if(curMenu == 1)
		{
			settingsMenu.draw(*common, 178, 20, false, curSel2);
			settingsMenuValues.draw(*common, 278, 20, false, curSel2);
		}
		else
		{
			settingsMenu.draw(*common, 178, 20, true);
			settingsMenuValues.draw(*common, 278, 20, true);
		}
		
		if(testSDLKeyOnce(SDLK_ESCAPE))
		{
			if(curMenu == 1)
				curMenu = 0;
			else
				curSel = 3;
		}
		
		if(testSDLKeyOnce(SDLK_UP))
		{
			sfx.play(26, -1);
			
			if(curMenu == 0)
			{
				--curSel;
				if(curSel < firstMenuItem)
				{
					curSel = int(mainMenu.items.size() - 1);
				}
			}
			else if(curMenu == 1)
			{
				--curSel2;
				if(curSel2 < 0)
				{
					curSel2 = int(settingsMenu.items.size() - 1);
				}
			}
		}
		
		if(testSDLKeyOnce(SDLK_DOWN))
		{
			sfx.play(25, -1);
			
			
			if(curMenu == 0)
			{
				++curSel;
				if(curSel >= int(mainMenu.items.size()))
				{
					curSel = firstMenuItem;
				}
			}
			else if(curMenu == 1)
			{
				++curSel2;
				
				if(curSel2 >= int(settingsMenu.items.size()))
				{
					curSel2 = 0;
				}
			}
		}

		if(testSDLKeyOnce(SDLK_RETURN)
		|| testSDLKeyOnce(SDLK_KP_ENTER))
		{
			sfx.play(27, -1);
			
			if(curMenu == 0)
			{
				if(curSel == 2)
				{
					curMenu = 1; // Go into settings menu
				}
				else // ED71
				{
					curMenu = 0;
					menuRunning = false;
				} // ED75
			}
			else if(curMenu == 1)
			{
				settingEnter(curSel2);
				updateSettingsMenu();
			}
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
					break;
				}
			}
		}

		if(curMenu == 1)
		{
			if(testSDLKey(SDLK_LEFT))
			{
				settingLeftRight(-1, curSel2);
				updateSettingsMenu();
			} // EDAE
			if(testSDLKey(SDLK_RIGHT))
			{
				settingLeftRight(1, curSel2);
				updateSettingsMenu();
			} // EDBF
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
	while(menuRunning);

	for(fadeValue = 32; fadeValue > 0; --fadeValue)
	{
		pal = origpal;
		pal.fade(fadeValue);
		flip(); // TODO: We should just screen sync and set the palette here
	} // EE36
	
	return curSel;
}


