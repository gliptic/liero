#include "game.hpp"
#include "reader.hpp"
#include "viewport.hpp"
#include "worm.hpp"
#include "filesystem.hpp"
#include "gfx/renderer.hpp"
#include "weapsel.hpp"
#include "constants.hpp"
#include <cstdlib>
//#include "text.hpp" // TEMP
#include <ctime>
#include "ai/predictive_ai.hpp"


/*
void Game::createDefaults()
{

}*/

Game::Game(
	gvl::shared_ptr<Common> common,
	gvl::shared_ptr<Settings> settingsInit,
	gvl::shared_ptr<SoundPlayer> soundPlayer)
: common(common)
, soundPlayer(soundPlayer)
, settings(settingsInit)
, statsRecorder(new NormalStatsRecorder)
, screenFlash(0)
, gotChanged(false)
, lastKilledIdx(-1)
, paused(true)
, level(*common)
, quickSim(false)
{
	rand.seed(uint32_t(std::time(0)));
	
	cycles = 0;
}

Game::~Game()
{
	clearViewports();
	clearWorms();
}

void Game::onKey(uint32_t key, bool state)
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

Worm* Game::findControlForKey(uint32_t key, Worm::Control& control)
{
	for(std::size_t i = 0; i < worms.size(); ++i)
	{
		Worm& w = *worms[i];
		
		uint32_t* controls = settings->extensions ? w.settings->controlsEx : w.settings->controls;
		std::size_t maxControl = settings->extensions ? WormSettings::MaxControlEx : WormSettings::MaxControl;
		for(std::size_t c = 0; c < maxControl; ++c)
		{
			if(controls[c] == key)
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
		viewports[i]->process(*this);
	}
}

void Game::drawViewports(Renderer& renderer, bool isReplay)
{
	for(std::size_t i = 0; i < viewports.size(); ++i)
	{
		viewports[i]->draw(*this, renderer, isReplay);
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

void Game::draw(Renderer& renderer, bool isReplay)
{
	drawViewports(renderer, isReplay);

	//common->font.drawText(toString(cycles / 70), 10, 10, 7);

	renderer.pal = renderer.origpal;

	for(int w = 0; w < 4; ++w)
		renderer.pal.rotateFrom(renderer.origpal, common->colorAnim[w].from, common->colorAnim[w].to, cycles >> 3);
	
	renderer.pal.fade(renderer.fadeValue);

	if(screenFlash > 0)
	{
		renderer.pal.lightUp(screenFlash);
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
		if(game.level.mat(cx, cy).dirtRock())
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
			
			common->sobjectTypes[7].create(*this, ix, iy, 0, 0);
			return;
		}
	} // 234F
	
	bonuses.free(bonus);
}

void Game::processFrame()
{
	statsRecorder->preTick(*this);

	if(screenFlash > 0)
		--screenFlash;
	
	for(std::size_t i = 0; i < viewports.size(); ++i)
	{
		if(viewports[i]->shake > 0)
			viewports[i]->shake -= 4000; // TODO: Read 4000 from exe?
	}
	
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
			
			if(wormByIdx(v.wormIdx)->killedTimer > 16)
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
	
	if(!common->H[HBonusDisable]
	&& settings->maxBonuses > 0
	&& rand(common->C[BonusDropChance]) == 0)
	{
		createBonus();
	}
		
	for(std::size_t i = 0; i < worms.size(); ++i)
	{
		worms[i]->process(*this);
	}
	
	for(std::size_t i = 0; i < worms.size(); ++i)
	{
		worms[i]->ninjarope.process(*worms[i], *this);
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

		Worm* lastKilledBy = wormByIdx(lastKilledIdx);
		
		if(!someInvisible
		&& lastKilledBy
		&& (cycles % 70) == 0
		&& lastKilledBy->timer < settings->timeToLose)
		{
			++lastKilledBy->timer;
		}
	}
	break;

	case Settings::GMHoldazone:
	{
		int contenderIdx = -1;
		int contenders = 0;

		for (Worm* w : worms)
		{
			int x = ftoi(w->x), y = ftoi(w->y);

			if (w->visible
			&& holdazone.rect.inside(x, y))
			{
				contenderIdx = w->index;
				++contenders;
			}
		}

		if (contenders == 0)
			contenderIdx = holdazone.holderIdx;

		if (contenders <= 1)
		{
			if (contenderIdx < 0 || (holdazone.contenderIdx != contenderIdx && holdazone.contenderFrames != 0))
			{
				if (holdazone.contenderFrames == 0 || --holdazone.contenderFrames == 0)
				{
					holdazone.contenderIdx = contenderIdx;
					holdazone.holderIdx = -1;
				}
			}
			else
			{
				holdazone.contenderIdx = contenderIdx;

				if (holdazone.contenderFrames < settings->zoneCaptureTime
				 && ++holdazone.contenderFrames >= settings->zoneCaptureTime
				 && holdazone.holderIdx != holdazone.contenderIdx)
				{
					// New holder
					
					int newTimeout = holdazone.timeoutLeft;
					if (holdazone.contenderIdx >= 0)
						newTimeout += settings->zoneTimeout * 70 / 4;
					else
						newTimeout += settings->zoneTimeout * 70 / 8;

					holdazone.timeoutLeft = std::min(newTimeout, settings->zoneTimeout * 70);

					holdazone.holderIdx = holdazone.contenderIdx;
				}
			}
		}

		bool dec = false;

		if (holdazone.holderIdx >= 0)
		{
			auto* holder = wormByIdx(holdazone.holderIdx);
				
			if ((cycles % 70) == 0)
				++holder->timer;

			dec = true;
		}
		else
		{
			dec = (cycles % 4) == 0;
		}

		if (dec)
		{
			if (--holdazone.timeoutLeft <= 0)
			{
				spawnZone();
			}
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
	
	statsRecorder->tick(*this);
}

void Game::focus(Renderer& renderer)
{
	updateSettings(renderer);
}

void Game::updateSettings(Renderer& renderer)
{
	renderer.origpal = level.origpal; // Activate the Level palette
	
	for(std::size_t i = 0; i < worms.size(); ++i)
	{
		Worm& worm = *worms[i];
		if(worm.index >= 0 && worm.index < 2)
			renderer.origpal.setWormColour(worm.index, *worm.settings);
	}
}

void Game::spawnZone()
{
	gvl::ivec2 pos;

	while (holdazone.zoneWidth >= 5)
	{
		if (level.selectSpawn(rand, holdazone.zoneWidth, holdazone.zoneHeight - 8, pos))
		{
			holdazone.rect.x1 = pos.x;
			holdazone.rect.y1 = pos.y;
			holdazone.rect.x2 = pos.x + holdazone.zoneWidth;
			holdazone.rect.y2 = pos.y + holdazone.zoneHeight;
			holdazone.timeoutLeft = settings->zoneTimeout * 70;
			holdazone.contenderIdx = -1;
			holdazone.contenderFrames = 0;
			holdazone.holderIdx = -1;
			break;
		}

		holdazone.zoneWidth /= 2;
		holdazone.zoneHeight /= 2;
	}
}

void Game::startGame()
{
	soundPlayer->play(22);
	bobjects.resize(settings->bloodParticleMax);

	if (settings->gameMode == Settings::GMHoldazone)
	{
		spawnZone();
	}
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
	else if(settings->gameMode == Settings::GMHoldazone)
	{
		for (auto* w : worms)
			if (w->timer >= settings->timeToLose)
				return true;
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
		if(game.level.mat(i, j).rock()) // TODO: The special rock respawn bug is here, consider an option to turn it off
			return false;
	}
	
	return true;
}

void Game::postClone(Game& original, bool complete)
{
	if (!complete)
	{
		statsRecorder.reset(new StatsRecorder);
		soundPlayer.reset(new NullSoundPlayer);
		viewports.clear();
	}
	else
	{
		statsRecorder.reset(new NormalStatsRecorder(static_cast<NormalStatsRecorder&>(*statsRecorder)));

		for (auto& vp : viewports)
		{
			vp = new Viewport(*vp);
		}
	}

	for (auto& w : worms)
	{
		w = new Worm(*w);
	}

}