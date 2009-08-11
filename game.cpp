#include "game.hpp"
#include "reader.hpp"
#include "viewport.hpp"
#include "worm.hpp"
#include "filesystem.hpp"
#include "gfx.hpp"
#include "sfx.hpp"
#include "weapsel.hpp"
#include "constants.hpp"
#include <cstdlib>
#include "text.hpp" // TEMP
#include <ctime>

#include <iostream>
#include <gvl/support/log.hpp> // TEMP

//Game game;


void Game::createDefaults()
{
	Worm* worm1 = new Worm(*this);
	worm1->settings = settings->wormSettings[0];
	worm1->health = worm1->settings->health;
	worm1->index = 0;
	if(worm1->settings->controller == 1)
		worm1->ai.reset(new DumbLieroAI(*worm1));
	
	Worm* worm2 = new Worm(*this);
	worm2->settings = settings->wormSettings[1];
	worm2->health = worm2->settings->health;
	worm2->index = 1;
	if(worm2->settings->controller == 1)
		worm2->ai.reset(new DumbLieroAI(*worm2));
	
	addViewport(new Viewport(Rect(0, 0, 158, 158), worm1, 0, 504, 350, *this));
	addViewport(new Viewport(Rect(160, 0, 158+160, 158), worm2, 218, 504, 350, *this));
	
	addWorm(worm1);
	addWorm(worm2);
}

Game::Game(gvl::shared_ptr<Common> common, gvl::shared_ptr<Settings> settingsInit)
: common(common)
, soundPlayer(new DefaultSoundPlayer)
, settings(settingsInit)
, screenFlash(0)
, gotChanged(false)
, lastKilled(0)
, paused(true)
{
	rand.seed(Uint32(std::time(0)));
	
	cycles = 0;
	
	// TODO: Unhardcode 40. Also, this loop makes loading time settings only take effect when
	// starting a new game. Although this emulates liero, consider changing it.
	// TODO: This also ties common to the settings, it really has to change.
	/*
	for(int w = 0; w < 40; ++w)
	{
		common->weapons[w].computedLoadingTime = (settings->loadingTime * common->weapons[w].loadingTime) / 100;
		if(common->weapons[w].computedLoadingTime == 0)
			common->weapons[w].computedLoadingTime = 1;
	}*/
	
	
}

Game::~Game()
{
	clearViewports();
	clearWorms();
}

void Game::onKey(Uint32 key, bool state)
{
	for(std::size_t i = 0; i < worms.size(); ++i)
	{
		Worm& w = *worms[i];
		
		for(std::size_t control = 0; control < WormSettings::MaxControl; ++control)
		{
			if(w.settings->controls[control] == key)
			{
				w.setControlState(static_cast<Worm::Control>(control), state);
			}
		}
	}
}

Worm* Game::findControlForKey(int key, Worm::Control& control)
{
	for(std::size_t i = 0; i < worms.size(); ++i)
	{
		Worm& w = *worms[i];
		
		for(std::size_t c = 0; c < WormSettings::MaxControl; ++c)
		{
			if(w.settings->controls[c] == key)
			{
				control = static_cast<Worm::Control>(c);
				return &w;
			}
		}
	}
	
	return 0;
}

void Game::releaseControls()
{
	for(std::size_t i = 0; i < worms.size(); ++i)
	{
		Worm& w = *worms[i];
		
		for(std::size_t control = 0; control < WormSettings::MaxControl; ++control)
		{
			w.release(static_cast<Worm::Control>(control));
		}
	}
}

void Game::clearViewports()
{
	for(std::size_t i = 0; i < viewports.size(); ++i)
		delete viewports[i];
	viewports.clear();
}

void Game::addViewport(Viewport* vp)
{
	//vp->worm->viewport = vp;
	viewports.push_back(vp);
}



void Game::processViewports()
{
	for(std::size_t i = 0; i < viewports.size(); ++i)
	{
		viewports[i]->process();
	}
}

void Game::drawViewports()
{
	for(std::size_t i = 0; i < viewports.size(); ++i)
	{
		viewports[i]->draw();
	}
}

void Game::clearWorms()
{
	for(std::size_t i = 0; i < worms.size(); ++i)
		delete worms[i];
	worms.clear();
}

void Game::resetWorms()
{
	for(std::size_t i = 0; i < worms.size(); ++i)
	{
		Worm& w = *worms[i];
		w.health = w.settings->health;
		w.lives = settings->lives; // Not in the original!
		w.kills = 0;
		w.visible = false;
		w.killedTimer = 150;
		
		w.currentWeapon = 1;
	}
}

void Game::addWorm(Worm* worm)
{
	worms.push_back(worm);
}

void Game::draw()
{
	drawViewports();

	//common->font.drawText(toString(cycles / 70), 10, 10, 7);
	
	gfx.pal = gfx.origpal;
	gfx.pal.fade(gfx.fadeValue);

	if(screenFlash > 0)
	{
		gfx.pal.lightUp(screenFlash);
	}
}

bool checkBonusSpawnPosition(Game& game, int x, int y)
{
	Common& common = *game.common;
	
	Rect rect(x - 2, y - 2, x + 3, y + 3);
	
	rect.intersect(game.level.rect());
	
	for(int cx = rect.x1; cx < rect.x2; ++cx)
	for(int cy = rect.y1; cy < rect.y2; ++cy)
	{
		if(common.materials[game.level.pixel(cx, cy)].dirtRock())
			return false;
	}
	
	return true;
}

void Game::createBonus()
{
	if(int(bonuses.size()) >= settings->maxBonuses)
		return;
		
	Bonus* bonus = bonuses.newObject();
	if(!bonus)
		return;
	
	for(std::size_t i = 0; i < 50000; ++i)
	{
		int ix = rand(common->C[BonusSpawnRectW]);
		int iy = rand(common->C[BonusSpawnRectH]);
		
		if(common->H[HBonusSpawnRect])
		{
			ix += common->C[BonusSpawnRectX];
			iy += common->C[BonusSpawnRectY];
		}
		
		if(checkBonusSpawnPosition(*this, ix, iy))
		{
			int frame;
			
			if(common->H[HBonusOnlyHealth])
				frame = 1;
			else if(common->H[HBonusOnlyWeapon])
				frame = 0;
			else
				frame = rand(2);
			
			bonus->x = itof(ix);
			bonus->y = itof(iy);
			bonus->velY = 0;
			bonus->frame = frame;
			bonus->timer = rand(common->bonusRandTimer[frame][1]) + common->bonusRandTimer[frame][0];
			
			if(frame == 0)
			{
				do
				{
					bonus->weapon = rand(40); // TODO: Unhardcode
				}
				while(settings->weapTable[bonus->weapon] == 2);
			}
			
			common->sobjectTypes[7].create(*this, ix, iy, 0);
			return;
		}
	} // 234F
	
	bonuses.free(bonus);
}

void Game::processFrame()
{
	if((cycles & 3) == 0)
	{
		for(int w = 0; w < 4; ++w)
		{
			gfx.origpal.rotate(common->colourAnim[w].from, common->colourAnim[w].to);
		}
	}
	
	if(screenFlash > 0)
		--screenFlash;
	
	
	
	for(std::size_t i = 0; i < viewports.size(); ++i)
	{
		if(viewports[i]->shake > 0)
			viewports[i]->shake -= 4000; // TODO: Read 4000 from exe?
	}
	
	/*
	// TODO: Move this stuff
	if(gfx.testSDLKeyOnce(SDLK_ESCAPE)
	&& !shutDown)
	{
		gfx.firstMenuItem = 0;
		gfx.fadeValue = 31;
		shutDown = true;
	}
*/
	
	for(BonusList::iterator i = bonuses.begin(); i != bonuses.end(); ++i)
	{
		i->process(*this);
	}
	
	if((cycles & 1) == 0)
	{
		for(std::size_t i = 0; i < viewports.size(); ++i)
		{
			Viewport& v = *viewports[i];
			
			bool down = false;
			
			if(v.worm->killedTimer > 16)
				down = true;
				
			if(down)
			{
				if(v.bannerY < 2)
					++v.bannerY;
			}
			else
			{
				if(v.bannerY > -8)
					--v.bannerY;
			}
		}
	}
	
	for(SObjectList::iterator i = sobjects.begin(); i != sobjects.end(); ++i)
	{
		i->process(*this);
	}
	
	// TODO: Check processing order of bonuses, wobjects etc.
	
	for(WObjectList::iterator i = wobjects.begin(); i != wobjects.end(); ++i)
	{
		i->process(*this);
	}
	
	for(NObjectList::iterator i = nobjects.begin(); i != nobjects.end(); ++i)
	{
		i->process(*this);
	}
	
	for(BObjectList::iterator i = bobjects.begin(); i != bobjects.end(); )
	{
		if(i->process(*this))
			++i;
		else
			bobjects.free(i);
	}
	
	// NOTE: This was originally the beginning of the processing, but has been rotated down to
	// separate out the drawing
	++cycles;
	
	// This can be moved after the drawing
	if(!common->H[HBonusDisable]
	&& settings->maxBonuses > 0
	&& rand(common->C[BonusDropChance]) == 0)
	{
		createBonus();
	}
		
	for(std::size_t i = 0; i < worms.size(); ++i)
	{
		worms[i]->process();
	}
	
	for(std::size_t i = 0; i < worms.size(); ++i)
	{
		worms[i]->ninjarope.process(*worms[i]);
	}
	
	switch(settings->gameMode)
	{
	case Settings::GMGameOfTag:
	{
		bool someInvisible = false;
		for(std::size_t i = 0; i < worms.size(); ++i)
		{
			if(!worms[i]->visible)
			{
				someInvisible = true;
				break;
			}
		}
		
		if(!someInvisible
		&& lastKilled
		&& (cycles % 70) == 0
		&& lastKilled->timer < settings->timeToLose)
		{
			++lastKilled->timer;
		}
	}
	break;
	}
	
	processViewports();
	
	// Store old control states so we can see what changes (mainly for replays)
	for(std::size_t i = 0; i < worms.size(); ++i)
	{
		worms[i]->prevControlStates = worms[i]->controlStates;
	}
	
}

void Game::gameLoop()
{
#if 0
	shutDown = false;
	
	do
	{
		processFrame();
		draw();
		
		gfx.flip();
		gfx.process(this);
	}
	while(gfx.fadeValue > 0);
	
	gfx.clearKeys();
	//releaseControls();
#endif
}

void Game::focus()
{
	updateSettings();
}

void Game::updateSettings()
{
	gfx.origpal = level.origpal; // Activate the Level palette
	
	for(std::size_t i = 0; i < worms.size(); ++i)
	{
		Worm& worm = *worms[i];
		if(worm.index >= 0 && worm.index < 2)
			gfx.origpal.setWormColour(worm.index, *worm.settings);
	}
}

void Game::startGame()
{
	soundPlayer->play(22);
}

bool Game::isGameOver()
{
	if(settings->gameMode == Settings::GMKillEmAll)
	{
		for(std::size_t i = 0; i < worms.size(); ++i)
		{
			if(worms[i]->lives <= 0)
				return true;
		}
	}
	else if(settings->gameMode == Settings::GMGameOfTag)
	{
		for(std::size_t i = 0; i < worms.size(); ++i)
		{
			if(worms[i]->timer >= settings->timeToLose)
				return true;
		}
	}
	else if(settings->gameMode == Settings::GMCtF
	|| settings->gameMode == Settings::GMSimpleCtF)
	{
		for(std::size_t i = 0; i < worms.size(); ++i)
		{
			if(worms[i]->flags >= settings->flagsToWin)
				return true;
		}
	}
	
	return false;
}

bool checkRespawnPosition(Game& game, int x2, int y2, int oldX, int oldY, int x, int y)
{
	Common& common = *game.common;
	
	int deltaX = oldX;
	int deltaY = oldY - y;
	int enemyDX = x2 - x;
	int enemyDY = y2 - y;
	
	if((std::abs(deltaX) <= common.C[WormMinSpawnDistLast] && std::abs(deltaY) <= common.C[WormMinSpawnDistLast])
	|| (std::abs(enemyDX) <= common.C[WormMinSpawnDistEnemy] && std::abs(enemyDY) <= common.C[WormMinSpawnDistEnemy]))
		return false;
		
	int maxX = x + 3;
	int maxY = y + 4;
	int minX = x - 3;
	int minY = y - 4;
	
	if(maxX >= game.level.width) maxX = game.level.width - 1;
	if(maxY >= game.level.height) maxY = game.level.height - 1;
	if(minX < 0) minX = 0;
	if(minY < 0) minY = 0;
	
	for(int i = minX; i != maxX; ++i)
	for(int j = minY; j != maxY; ++j)
	{
		if(common.materials[game.level.pixel(i, j)].rock()) // TODO: The special rock respawn bug is here, consider an option to turn it off
			return false;
	}
	
	return true;
}
