#include "weapsel.hpp"
#include "gfx.hpp"
#include "game.hpp"
#include "worm.hpp"
#include "text.hpp"
#include "menu/menu.hpp"
#include "mixer/player.hpp"
#include "viewport.hpp"
#include <SDL/SDL.h>

WeaponSelection::WeaponSelection(Game& game)
: game(game)
, enabledWeaps(0)
, isReady(game.viewports.size())
, menus(game.viewports.size())
, cachedBackground(false)
, focused(true)
{
	Common& common = *game.common;
	
	for(int i = 0; i < 40; ++i)
	{
		if(game.settings->weapTable[i] == 0)
			++enabledWeaps;
	}
	
	for(std::size_t i = 0; i < menus.size(); ++i)
	{
		bool weapUsed[256] = {};
		
		Viewport& vp = *game.viewports[i];
		
		Worm& worm = *game.wormByIdx(vp.wormIdx);
		WormSettings& ws = *worm.settings;
		
		menus[i].items.push_back(MenuItem(57, 57, LS(Randomize)));
		
		{
			int x = vp.rect.center_x() - 31;
			int y = vp.rect.center_y() - 51;
			menus[i].place(x, y);
		}

		bool randomWeapons = (ws.controller != 0 && game.settings->selectBotWeapons == 0);
		
		for(int j = 0; j < Settings::selectableWeapons; ++j)
		{
			if(ws.weapons[j] == 0 || randomWeapons)
			{
				ws.weapons[j] = gfx.rand(1, 41);
			}
			
			bool enoughWeapons = (enabledWeaps >= Settings::selectableWeapons);
			
			if (game.settings->weapTable[common.weapOrder[ws.weapons[j]]] > 0)
			{
				while (true)
				{
					ws.weapons[j] = gfx.rand(1, 41);

					int w = common.weapOrder[ws.weapons[j]];

					if((!enoughWeapons || !weapUsed[w])
					&& game.settings->weapTable[w] <= 0)
						break;
				}
			}
			
			int w = common.weapOrder[ws.weapons[j]];
			
			weapUsed[w] = true;
			
			WormWeapon& ww = worm.weapons[j];
			
			ww.ammo = 0;
			ww.id = w;
			
			menus[i].items.push_back(MenuItem(48, 48, common.weapons[w].name));
		}
		
		menus[i].items.push_back(MenuItem(10, 10, LS(Done)));
		
		worm.currentWeapon = 0;
		
		menus[i].moveToFirstVisible();
		isReady[i] = (ws.controller != 0 && game.settings->selectBotWeapons != 1);
	}
}
	
void WeaponSelection::draw()
{
	Common& common = *game.common;
	
	if(!cachedBackground)
	{
		game.draw(gfx);
			
		if(game.settings->levelFile.empty())
		{
			common.font.drawText(gfx.screenBmp, LS(LevelRandom), 0, 162, 50);
		}
		else
		{
			common.font.drawText(gfx.screenBmp, (LS(LevelIs1) + game.settings->levelFile + LS(LevelIs2)), 0, 162, 50);
		}
		
		std::memcpy(&gfx.frozenScreen[0], gfx.screenBmp.pixels, gfx.frozenScreen.size());
		cachedBackground = true;
	}
	
	std::memcpy(gfx.screenBmp.pixels, &gfx.frozenScreen[0], gfx.frozenScreen.size());

	if(!focused)
		return;
		

	drawRoundedBox(gfx.screenBmp, 114, 2, 0, 7, common.font.getDims(LS(SelWeap)));
	
	common.font.drawText(gfx.screenBmp, LS(SelWeap), 116, 3, 50);
		
	for(std::size_t i = 0; i < menus.size(); ++i)
	{
		Menu& weaponMenu = menus[i];
		
		Viewport& vp = *game.viewports[i];
		
		Worm& worm = *game.wormByIdx(vp.wormIdx);
		WormSettings& ws = *worm.settings;
		
		int width = common.font.getDims(ws.name);
		drawRoundedBox(gfx.screenBmp, weaponMenu.x + 29 - width/2, weaponMenu.y - 11, 0, 7, width);
		common.font.drawText(gfx.screenBmp, ws.name, weaponMenu.x + 31 - width/2, weaponMenu.y - 10, ws.color + 1);
		
		if(!isReady[i])
		{
			menus[i].draw(common, false);
		}
	}
	
	// TODO: This just uses the currently activated palette, which might well be wrong.
	gfx.pal = gfx.origpal;
	gfx.pal.rotateFrom(gfx.origpal, 168, 174, gfx.menuCycles);
	gfx.pal.fade(gfx.fadeValue);
	++gfx.menuCycles;
}
	
bool WeaponSelection::processFrame()
{
	Common& common = *game.common;
	
	bool allReady = true;
	
	for(std::size_t i = 0; i < menus.size(); ++i)
	{
		int weapID = menus[i].selection() - 1;
		
		Viewport& vp = *game.viewports[i];
		Worm& worm = *game.wormByIdx(vp.wormIdx);
		
		WormSettings& ws = *worm.settings;
		
		if(!isReady[i])
		{
			//menus[i].draw(common, ws.selWeapX - 2, 28, false, curSel[i]);

			if(weapID >= 0 && weapID < Settings::selectableWeapons)
			{
				if(worm.pressed(Worm::Left))
				{
					worm.release(Worm::Left);
					
					game.soundPlayer->play(25);
					
					do
					{
						--ws.weapons[weapID];
						if(ws.weapons[weapID] < 1)
							ws.weapons[weapID] = 40; // TODO: Unhardcode
					}
					while(game.settings->weapTable[common.weapOrder[ws.weapons[weapID]]] != 0);
					
					int w = common.weapOrder[ws.weapons[weapID]];
					worm.weapons[weapID].id = w;
					menus[i].selected()->string = common.weapons[w].name;
				}
				
				if(worm.pressed(Worm::Right))
				{
					worm.release(Worm::Right);
					
					game.soundPlayer->play(26);
					
					do
					{
						++ws.weapons[weapID];
						if(ws.weapons[weapID] > 40)
							ws.weapons[weapID] = 1; // TODO: Unhardcode
					}
					while(game.settings->weapTable[common.weapOrder[ws.weapons[weapID]]] != 0);
					
					int w = common.weapOrder[ws.weapons[weapID]];
					worm.weapons[weapID].id = w;
					menus[i].selected()->string = common.weapons[w].name;
				}
			}
			
			if(worm.pressedOnce(Worm::Up))
			{
				game.soundPlayer->play(26);
				/*
				int s = int(menus[i].items.size());
				curSel[i] = (curSel[i] - 1 + s) % s;*/
				menus[i].movement(-1);
			}
			
			if(worm.pressedOnce(Worm::Down))
			{
				game.soundPlayer->play(25);
				/*
				int s = int(menus[i].items.size());
				curSel[i] = (curSel[i] + 1 + s) % s;
				*/
				menus[i].movement(1);
			}
			
			if(worm.pressed(Worm::Fire))
			{
				if(menus[i].selection() == 0)
				{
					bool weapUsed[256] = {};
					
					bool enoughWeapons = (enabledWeaps >= Settings::selectableWeapons);
					
					for(int j = 0; j < Settings::selectableWeapons; ++j)
					{
						while(true)
						{
							ws.weapons[j] = gfx.rand(1, 41);
							
							int w = common.weapOrder[ws.weapons[j]];
							
							if((!enoughWeapons || !weapUsed[w])
							&& game.settings->weapTable[w] <= 0)
								break;
						}
						
						int w = common.weapOrder[ws.weapons[j]];
						
						weapUsed[w] = true;
						
						//WormWeapon& ww = worm.weapons[j];
						
						menus[i].items[j + 1].string = common.weapons[w].name;
					}
				}
				else if(menus[i].selection() == 6) // TODO: Unhardcode
				{
					game.soundPlayer->play(27);
					isReady[i] = true;
				}
			}
		}
		
		allReady = allReady && isReady[i];
	}
	
	return allReady;
}


	
void WeaponSelection::finalize()
{
	for(std::size_t i = 0; i < game.worms.size(); ++i)
	{
		Worm& worm = *game.worms[i];
		
		worm.initWeapons(game);
		/*
		for(int j = 0; j < 6; ++j)
		{
			gfx.releaseKey(worm.settings->controls[j]);
		}*/
	}
	game.releaseControls();
	
	// TODO: Make sure the weapon selection is transfered back to Gfx to be saved
}

void WeaponSelection::focus()
{
	focused = true;
}

void WeaponSelection::unfocus()
{
	focused = false;
}

#if 0
void selectWeapons(Game& game)
{
	WeaponSelection ws(game);
	
	while(!ws.processFrame())
	{
		ws.draw();
		gfx.flip();
		gfx.process(&game);
	}
	
	ws.finalize();
	// Important that escape isn't released here
}
#endif
