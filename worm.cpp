#include "worm.hpp"
#include "game.hpp"
#include "sfx.hpp"
#include "gfx.hpp"
//#include "viewport.hpp"
#include "constants.hpp"
#include "console.hpp"
#include "reader.hpp" // TODO: For lieroEXERoot. We should move that into Common.
#include "filesystem.hpp" // For joinPath
#include <cstdlib>
#include <iostream>


#include <gvl/serialization/context.hpp>
#include <gvl/serialization/archive.hpp>
#include "replay.hpp"

#include <gvl/crypt/gash.hpp>
#include <gvl/io/fstream.hpp>

struct Point
{
	int x, y;
};

gvl::gash::value_type& WormSettings::updateHash()
{
	GameSerializationContext context;
	gvl::hash_accumulator<gvl::gash> ha;
	
	
	archive(gvl::out_archive<GameSerializationContext, gvl::hash_accumulator<gvl::gash> >(ha, context), *this);
	
	ha.flush();
	hash = ha.final();
	return hash;
}

void WormSettings::saveProfile(std::string const& newProfileName)
{
	try
	{
		std::string path(joinPath(lieroEXERoot, newProfileName) + ".lpf");
		gvl::stream_ptr str(new gvl::fstream(path.c_str(), "wb"));
		
		gvl::stream_writer writer(str);
		
		profileName = newProfileName;
		GameSerializationContext context;
		archive(gvl::out_archive<GameSerializationContext>(writer, context), *this);
	}
	catch(gvl::stream_error& e)
	{
		Console::writeWarning(std::string("Stream error saving profile: ") + e.what());
	}
}

void WormSettings::loadProfile(std::string const& newProfileName)
{
	int oldColor = colour;
	try
	{
		std::string path(joinPath(lieroEXERoot, newProfileName) + ".lpf");
		gvl::stream_ptr str(new gvl::fstream(path.c_str(), "rb"));
		
		gvl::stream_reader reader(str);

		profileName = newProfileName;
		GameSerializationContext context;
		archive(gvl::in_archive<GameSerializationContext>(reader, context), *this);
	}
	catch(gvl::stream_error& e)
	{
		Console::writeWarning(std::string("Stream error loading profile: ") + e.what());
		Console::writeWarning("The profile may just be old, in which case there is nothing to worry about");
	}
	
	colour = oldColor;  // We preserve the color
}

void Worm::calculateReactionForce(int newX, int newY, int dir)
{
	Common& common = *game.common;
	
	static Point const colPoints[4][7] =
	{
		{ //DOWN reaction points
			{-1, -4},
			{ 0, -4},
			{ 1, -4},
			{ 0,  0},
			{ 0,  0},
			{ 0,  0},
			{ 0,  0}
		},
		{ //LEFT reaction points
			{1, -3},
			{1, -2},
			{1, -1},
			{1,  0},
			{1,  1},
			{1,  2},
			{1,  3}
		},
		{ //UP reaction points
			{-1, 4},
			{ 0, 4},
			{ 1, 4},
			{ 0, 0},
			{ 0, 0},
			{ 0, 0},
			{ 0, 0}
		},
		{ //RIGHT reaction points
			{-1, -3},
			{-1, -2},
			{-1, -1},
			{-1,  0},
			{-1,  1},
			{-1,  2},
			{-1,  3}
		}
		
	};

	static int const colPointCount[4] =
	{
		3,
		7,
		3,
		7
	};

	reacts[dir] = 0;
	
	// newX should be x + velX at the first call
	
	for(int i = 0; i < colPointCount[dir]; ++i)
	{
		int colX = newX + colPoints[dir][i].x;
		int colY = newY + colPoints[dir][i].y;
		
#if 0
		if(!game.level.inside(colX, colY) // TODO: Liero seems to not make any check here, checking garbage pixels
		|| !common.materials[game.level.pixel(colX, colY)].background())
#else
		// This should emulate Liero better
		PalIdx pix = game.level.checkedPixelWrap(colX, colY);
		if(!common.materials[pix].background())
#endif
		{
			++reacts[dir];
		}
	}
}

void Worm::processPhysics()
{
	Common& common = *game.common;
	
	if(reacts[RFUp] > 0)
	{
		velX = (velX * common.C[WormFricMult]) / common.C[WormFricDiv];
	}
	
	if(velX > 0)
	{
		if(reacts[RFLeft] > 0)
		{
			if(velX > common.C[MinBounceRight])
			{
				if(common.H[HFallDamage])
					health -= common.C[FallDamageRight];
				else
					game.soundPlayer->play(14);
				velX = -velX / 3;
			}
			else
				velX = 0;
		}
	}
	else if(velX < 0)
	{
		if(reacts[RFRight])
		{
			if(velX < common.C[MinBounceLeft])
			{
				if(common.H[HFallDamage])
					health -= common.C[FallDamageLeft];
				else
					game.soundPlayer->play(14);
				velX = -velX / 3;
			}
			else
				velX = 0;
		}
	}
	
	if(velY > 0)
	{
		if(reacts[RFUp] > 0)
		{
			if(velY > common.C[MinBounceDown])
			{
				if(common.H[HFallDamage])
					health -= common.C[FallDamageDown];
				else
					game.soundPlayer->play(14);
				velY = -velY / 3;
			}
			else
				velY = 0;
		}
	}
	else if(velY < 0)
	{
		if(reacts[RFDown])
		{
			if(velY < common.C[MinBounceUp])
			{
				if(common.H[HFallDamage])
					health -= common.C[FallDamageUp];
				else
					game.soundPlayer->play(14);
				velY = -velY / 3;
			}
			else
				velY = 0;
		}
	}
	
	if(reacts[RFUp] == 0)
	{
		velY += common.C[WormGravity];
	}
	
	if(velX >= 0)
	{
		if(reacts[RFLeft] < 2)
			x += velX;
	}
	else
	{
		if(reacts[RFRight] < 2)
			x += velX;
	}
	
	if(velY >= 0)
	{
		if(reacts[RFUp] < 2)
			y += velY;
	}
	else
	{
		if(reacts[RFDown] < 2)
			y += velY;
	}
}

void Worm::process()
{
	Common& common = *game.common;
	
	if(health > settings->health)
		health = settings->health;
	
	if(game.settings->gameMode != Settings::GMKillEmAll
	|| lives > 0)
	{
		if(visible)
		{
			// Liero.exe: 291C
			
			fixed nextX = x + velX;
			fixed nextY = y + velY;
			
			int iNextX = ftoi(nextX);
			int iNextY = ftoi(nextY);
			
			{ // Calculate reaction forces

				for(int i = 0; i < 4; i++)
				{
					calculateReactionForce(iNextX, iNextY, i);
					
					// Yes, Liero does this in every iteration. Keep it this way.
					
					
					if(iNextX < 4)
					{
						reacts[RFRight] += 5;
					}
					else if(iNextX > game.level.width - 5)
					{
						reacts[RFLeft] += 5;
					}

					if(iNextY < 5)
					{
						reacts[RFDown] += 5;
					}
					else
					{
						if(common.H[HWormFloat])
						{
							if(iNextY > common.C[WormFloatLevel])
								velY -= common.C[WormFloatPower];
						}
						else if(iNextY > game.level.height - 6)
						{
							reacts[RFUp] += 5;
						}
					}
				}

				if(reacts[RFDown] < 2)
				{
					if(reacts[RFUp] > 0)
					{
						if(reacts[RFLeft] > 0 || reacts[RFRight] > 0)
						{
							//Low or none push down,
							//Push up and
							//Push left or right

							y -= itof(1);
							nextY = y + velY;
							iNextY = ftoi(nextY);

							calculateReactionForce(iNextX, iNextY, RFLeft);
							calculateReactionForce(iNextX, iNextY, RFRight);
						}
					}
				}

				if(reacts[RFUp] < 2)
				{
					if(reacts[RFDown] > 0)
					{
						if(reacts[RFLeft] > 0 || reacts[RFRight] > 0)
						{
							//Low or none push up,
							//Push down and
							//Push left or right

							y += itof(1);
							nextY = y + velY;
							iNextY = ftoi(nextY);

							calculateReactionForce(iNextX, iNextY, RFLeft);
							calculateReactionForce(iNextX, iNextY, RFRight);
						}
					}
				}
			}
			
			int ix = ftoi(x);
			int iy = ftoi(y);

			for(Game::BonusList::iterator i = game.bonuses.begin(); i != game.bonuses.end(); ++i)
			{				
				if(ix + 5 > ftoi(i->x)
				&& ix - 5 < ftoi(i->x)
				&& iy + 5 > ftoi(i->y)
				&& iy - 5 < ftoi(i->y))
				{
					if(i->frame == 1)
					{
						if(health < settings->health)
						{
							game.bonuses.free(i);
							health += (game.rand(common.C[BonusHealthVar]) + common.C[BonusMinHealth]) * settings->health / 100; // TODO: Read from EXE
							if(health > settings->health)
								health = settings->health;
						}
					}
					else if(i->frame == 0)
					{
						if(game.rand(common.C[BonusExplodeRisk]) > 1)
						{
							WormWeapon& ww = weapons[currentWeapon];
							
							if(!common.H[HBonusReloadOnly])
							{
								fireConeActive = false;
								fireCone = -1;
								
								ww.id = i->weapon;
								ww.ammo = common.weapons[ww.id].ammo;
							}
							
							game.soundPlayer->play(24);
							
							game.bonuses.free(i);
							
							ww.available = true;
							ww.loadingLeft = 0;
						}
						else
						{
							int bix = ftoi(i->x);
							int biy = ftoi(i->y);
							game.bonuses.free(i);
							common.sobjectTypes[0].create(game, bix, biy, this);
						}
					}
				}
			}

			processSteerables();
			
			if(!movable && !pressed(Left) && !pressed(Right)) // processSteerables sets movable to false, does this interfer?
			{
				movable = true;
			} // 2FB1
			
			processAiming();
			processTasks();
			processWeapons();
			
			if(pressed(Fire) && !pressed(Change)
			&& weapons[currentWeapon].available
			&& weapons[currentWeapon].delayLeft <= 0)
			{
				fire();
			}
			else
			{
				if(common.weapons[weapons[currentWeapon].id].loopSound)
					game.soundPlayer->stop(&weapons[currentWeapon]);
			}

			processPhysics();
			processSight();
			
			if(pressed(Change))
			{
				processWeaponChange();
			}
			else
			{
				keyChangePressed = false;
				processMovement();
			}


			if(health < settings->health / 4)
			{
				if(game.rand(health + 6) == 0)
				{
					if(game.rand(3) == 0)
					{
						int snd = 18 + game.rand(3); // NOTE: MUST be outside the unpredictable branch below
						if(!game.soundPlayer->isPlaying(this))
						{
							game.soundPlayer->play(snd, this);
						}
					}
					
					common.nobjectTypes[6].create1(game, velX, velY, x, y, 0, this);
				}
			}
			
			if(health <= 0)
			{
				/* TODO
				//Kill him!
				if(worm->flag != 0)
				{
					//He got the flag!
					long flag;
					if(w == 0)
						flag = 21;
					else
						flag = 20;
    
					//Create the flag
					CreateObject1(
						worm->m_fXVel,
						worm->m_fYVel,
						worm->m_fX,
						worm->m_fY,
						0,
						flag,
    BYTE(w)
					);
					worm->flag = 0;
				} // 468D
				*/
				
				leaveShellTimer = 0;
				makeSightGreen = false;
				// TODO: cGame::cWorm[w^1].makesightgreen = 0;
				
				
				Weapon& w = common.weapons[weapons[currentWeapon].id];
				if(w.loopSound)
				{
					game.soundPlayer->stop(&weapons[currentWeapon]);
				}
				
				int deathSnd = 16 + game.rand(3);
				game.soundPlayer->play(deathSnd, this);
				
				fireConeActive = 0;
				ninjarope.out = false;
				--lives;
				Worm* oldLastKilled = game.lastKilled;
				// For GameOfTag, 'it' doesn't change if the killer
				// was not 'it', itself, unknown or there were no 'it'.
				if(game.settings->gameMode != Settings::GMGameOfTag
				|| !game.lastKilled
				|| !lastKilledBy
				|| lastKilledBy == this
				|| lastKilledBy == game.lastKilled)
				{
					game.lastKilled = this;
				}
				game.gotChanged = (oldLastKilled != game.lastKilled);
				
				if(lastKilledBy && lastKilledBy != this)
				{
					++lastKilledBy->kills;
				}
				
				visible = false;
				killedTimer = 150;
				
				int max = 120 * game.settings->blood / 100;
				
				if(max > 1)
				{
					for(int i = 1; i <= max; ++i)
					{
						common.nobjectTypes[6].create2(
							game,
							game.rand(128),
							velX / 3, velY / 3,
							x, y,
							0,
							this);
					}
				}
				
#if 1
				for(int i = 7; i <= 105; i += 14)
				{
					common.nobjectTypes[index].create2(
						game,
						i + game.rand(14),
						velX / 3, velY / 3,
						x, y,
						0,
						this);
				}
#endif

				release(Fire);				
			}
			
			// Update frame
			if(animate)
				currentFrame = angleFrame() + game.settings->wormAnimTab[(game.cycles & 31) >> 3];
			else
				currentFrame = angleFrame() + game.settings->wormAnimTab[0];
		}
		else
		{
			// Worm is dead
			
			if(pressedOnce(Fire))
			{
				ready = true;
			}
			
			if(killedTimer > 0)
				--killedTimer;
				
			if(killedTimer == 0)
				beginRespawn();
				
			if(killedTimer < 0)
				doRespawning();
		}
	}
	
	
}

int Worm::angleFrame() const
{
	int x = ftoi(aimingAngle) - 12;
	
	if(direction != 0)
		x -= 49;
		
	x >>= 3;
	if(x < 0) x = 0;
	else if(x > 6) x = 6;

	if(direction != 0)
	{
		x = 6 - x;
	} // 9581
	
	return x;
}

int sqrVectorLength(int x, int y)
{
	return x*x + y*y;
}



void DumbLieroAI::process()
{
	Game& game = worm.game;
	Common& common = *game.common;
	
#if 0
	// TEMP TEST

	for(int i = 0; i < Worm::MaxControl; ++i)
	{
		worm.setControlState((Worm::Control)i, rand(3) == 0);
	}
	/*
	if(!worm.ready)
		worm.setControlState(Worm::Fire, true);
	*/
	return;
#endif
	
	Worm* target = 0;
	int minLen = 0;
	for(std::size_t i = 0; i < game.worms.size(); ++i)
	{
		Worm* w = game.worms[i];
		if(w != &worm)
		{
			int len = sqrVectorLength(ftoi(worm.x) - ftoi(w->x), ftoi(worm.y) - ftoi(w->y));
			if(!target || len < minLen) // First or closer worm
			{
				target = w;
				minLen = len;
			}
		}
	}
	
	int maxDist;
	
	WormWeapon& ww = worm.weapons[worm.currentWeapon];
	Weapon& w = common.weapons[ww.id];
	
	if(w.timeToExplo > 0 && w.timeToExplo < 500)
	{
		maxDist = (w.timeToExplo - w.timeToExploV / 2) * w.speed / 130;
	}
	else
	{
		maxDist = w.speed - w.gravity / 10;
	} // 4D43
	
	if(maxDist < 90)
		maxDist = 90;
		
	fixed deltaX = target->x - worm.x;
	fixed deltaY = target->y - worm.y;
	int ideltaX = ftoi(deltaX);
	int ideltaY = ftoi(deltaY);
		
	int realDist = vectorLength(ideltaX, ideltaY);
	
	if(realDist < maxDist || !worm.visible)
	{
		// The other worm is close enough
		bool fire = worm.pressed(Worm::Fire);
		if(rand(common.aiParams.k[fire][WormSettings::Fire]) == 0)
		{
			worm.setControlState(Worm::Fire, !fire);
		} // 4DE7
	}
	else if(worm.visible)
	{
		worm.release(Worm::Fire);
	} // 4DFA
		
	// In Liero this is a loop with two iterations, that's better maybe
	bool jump = worm.pressed(Worm::Jump);
	if(rand(common.aiParams.k[jump][WormSettings::Jump]) == 0)
	{
		worm.toggleControlState(Worm::Jump);
	}
	
	bool change = worm.pressed(Worm::Change);
	if(rand(common.aiParams.k[change][WormSettings::Change]) == 0)
	{
		worm.toggleControlState(Worm::Change);
	}

//l_4E6B:
	// Moves up
   
// l_4EE5:
	if(realDist > 0)
	{
		deltaX /= realDist;
		deltaY /= realDist;
	}
	else
	{
		deltaX = 0;
		deltaY = 0;
	} // 4F2F
	
	int dir = 1;
	
	for(; dir < 128; ++dir)
	{
		if(std::abs(cosTable[dir] - deltaX) < 0xC00
		&& std::abs(sinTable[dir] - deltaY) < 0xC00) // The original had 0xC000, which is wrong
			break;
	} // 4F93
	
	fixed adeltaX = std::abs(deltaX);
	fixed adeltaY = std::abs(deltaY);

	if(dir >= 128)
	{
		if(deltaX > 0)
		{
			if(deltaY < 0)
			{
				if(adeltaY > adeltaX)
					dir = 64 + rand(16);
				else if(adeltaX > adeltaY)
					dir = 80 + rand(16);
				else
					dir = 80;
			}
			else // deltaY >= 0
			{
				if(adeltaX > adeltaY)
					dir = 96 + rand(16);
				else
					dir = 116;
			}
		}
		else
		{
			if(deltaY < 0)
			{
				
				if(adeltaY > adeltaX)
					dir = 48 + rand(16);
				else if(adeltaX > adeltaY)
					dir = 32 + rand(16);
				else
					dir = 48; // This was 56, but that seems wrong
			}
			else // deltaX <= 0 && deltaY >= 0
			{
				if(adeltaX > adeltaY)
					dir = 12 + rand(16);
				else
					dir = 12;
			}
		}
	} // 50FD
   
  
/* TODO (maybe)
   if(realdist < maxdist)
   {
	if(dir < 64)
	{
 l_510E:
	 //What the hell is wrong with this code?
	 //It is messed up totaly! Translating the correct code
	 //NOTE! Something has to be done here!
	 dir += ax; //What the hell is AX?
	 if(dir > 64)
	 {
	  dir = 64;
	 }
	} // 5167
	if(dir > 64)
	{
	 //The same thing with this code! Is it encrypted or what?
	 dir -= ax; //Again
	 if(dir < 64)
	 {
	  dir = 64;
	 }
	}
   } // 51C6
*/

	change = worm.pressed(Worm::Change);
	
	if(change)
	{
		if(rand(common.aiParams.k[worm.pressed(Worm::Left)][WormSettings::Left]) == 0)
		{
			worm.toggleControlState(Worm::Left);
		}
		
		if(rand(common.aiParams.k[worm.pressed(Worm::Right)][WormSettings::Right]) == 0)
		{
			worm.toggleControlState(Worm::Right);
		}
		
		if(worm.ninjarope.out && worm.ninjarope.attached)
		{
// l_525F:
			bool up = worm.pressed(Worm::Up);
			
			if(rand(common.aiParams.k[up][WormSettings::Up]) == 0)
			{
				worm.toggleControlState(Worm::Up);
			}
			
			bool down = worm.pressed(Worm::Down);
			if(rand(common.aiParams.k[down][WormSettings::Down]) == 0)
			{
				worm.toggleControlState(Worm::Down);
			}
		}
		else
		{
// l_52D2:
			worm.release(Worm::Up);
			worm.release(Worm::Down);
		} // 52F8
	} // if(change)
	else
	{
	
		if(realDist > maxDist)
		{
			worm.setControlState(Worm::Right, (deltaX > 0));
			worm.setControlState(Worm::Left, (deltaX <= 0));
		} // 5347
		else
		{
			worm.release(Worm::Right);
			worm.release(Worm::Left);
		}

		if(worm.direction != 0)
		{
			if(dir < 64)
				worm.press(Worm::Left);
			// 5369
			worm.setControlState(Worm::Up,   (dir + 1 < ftoi(worm.aimingAngle)));
			// 5379
			worm.setControlState(Worm::Down, (dir - 1 > ftoi(worm.aimingAngle)));
		}
		else
		{
			if(dir > 64)
				worm.press(Worm::Right);
			// 53C6
			worm.setControlState(Worm::Up,   (dir - 1 > ftoi(worm.aimingAngle)));
			// 53E8
			worm.setControlState(Worm::Down, (dir + 1 < ftoi(worm.aimingAngle)));
			// 540A
		}
		
		if(worm.pressed(Worm::Left)
		&& worm.reacts[Worm::RFRight])
		{
			if(worm.reacts[Worm::RFDown] > 0)
				worm.press(Worm::Right);
			else
				worm.press(Worm::Jump);
		} // 5454
		
		if(worm.pressed(Worm::Right)
		&& worm.reacts[Worm::RFLeft])
		{
			if(worm.reacts[Worm::RFDown] > 0)
				worm.press(Worm::Left);
			else
				worm.press(Worm::Jump);
		} // 549E
	}
}

void Worm::initWeapons()
{
	Common& common = *game.common;
	currentWeapon = 0; // It was 1 in OpenLiero A1
	
	for(int j = 0; j < game.settings->selectableWeapons; ++j)
	{
		WormWeapon& ww = weapons[j];
		ww.id = common.weapOrder[settings->weapons[j]];
		ww.ammo = common.weapons[weapons[j].id].ammo;
		ww.delayLeft = 0;
		ww.loadingLeft = 0;
		ww.available = true;
	}
}

void Worm::beginRespawn()
{
	Common& common = *game.common;
	
	int tempX = ftoi(x);
	int tempY = ftoi(y);

	logicRespawnX = tempX - 80;
	logicRespawnY = tempY - 80;
	
	int enemyX = tempX;
	int enemyY = tempY;
	
	if(game.worms.size() == 2)
	{
		enemyX = ftoi(game.worms[index ^ 1]->x);
		enemyY = ftoi(game.worms[index ^ 1]->y);
	}

	int trials = 0;
	do
	{
		x = itof(common.C[WormSpawnRectX] + game.rand(common.C[WormSpawnRectW]));
		y = itof(common.C[WormSpawnRectY] + game.rand(common.C[WormSpawnRectH]));

		// The original didn't have + 4 in both, which seems
		// to be done in the exe and makes sense.
		while(ftoi(y) + 4 < game.level.height
		&& common.materials[game.level.pixel(ftoi(x), ftoi(y) + 4)].background())
		{
			y += itof(1);
		}
		
		if(++trials >= 50000)
		{
			Console::writeWarning("Couldn't find a suitable spawn position in time");
			break;
		}
	}
	while(!checkRespawnPosition(game, enemyX, enemyY, tempX, tempY, ftoi(x), ftoi(y)));

	
			
	killedTimer = -1;
}

void limitXY(int& x, int& y, int maxX, int maxY)
{
	if(x < 0)
		x = 0;
	else if(x > maxX)
		x = maxX;

	if(y < 0)
		y = 0;
	if(y > maxY)
		y = maxY;
}

void Worm::doRespawning()
{
	Common& common = *game.common;

	for(int c = 0; c < 4; c++)
	{
		if(logicRespawnX < ftoi(x) - 80) ++logicRespawnX;
		else if(logicRespawnX > ftoi(x) - 80) --logicRespawnX;

		if(logicRespawnY < ftoi(y) - 80) ++logicRespawnY;
		else if(logicRespawnY > ftoi(y) - 80) --logicRespawnY;
	}

	limitXY(logicRespawnX, logicRespawnY, game.level.width - 158, game.level.height - 158);
	
	int destX = ftoi(x) - 80;
	int destY = ftoi(y) - 80;
	limitXY(destX, destY, game.level.width - 158, game.level.height - 158);

	if(logicRespawnX < destX + 5
	&& logicRespawnX > destX - 5
	&& logicRespawnY < destY + 5
	&& logicRespawnY > destY - 5
	&& ready)
	{
		int ix = ftoi(x), iy = ftoi(y);
		drawDirtEffect(common, game.rand, game.level, 0, ix - 7, iy - 7);
		if(game.settings->shadow)
			correctShadow(common, game.level, Rect(ix - 10, iy - 10, ix + 11, iy + 11));
		
		ready = false;
		game.soundPlayer->play(21);
		
		visible = true;
		fireConeActive = 0;
		velX = 0;
		velY = 0;
		health = settings->health;
		
		// NOTE: This was done at death before, but doing it here seems to make more sense
		if(game.rand() & 1)
		{
			aimingAngle = itof(32);
			direction = 0;
		}
		else
		{
			aimingAngle = itof(96);
			direction = 1;
		}
	}
}

void Worm::processWeapons()
{
	Common& common = *game.common;
	
	for(int i = 0; i < game.settings->selectableWeapons; ++i)
	{
		if(weapons[i].delayLeft >= 0)
			--weapons[i].delayLeft;
	}
	
	WormWeapon& ww = weapons[currentWeapon];
	Weapon& w = common.weapons[ww.id];
	
	if(ww.ammo <= 0)
	{
		ww.available = false;
		int computedLoadingTime = w.computedLoadingTime(*game.settings);
		ww.loadingLeft = computedLoadingTime;
		ww.ammo = w.ammo;
	}
	
	if(ww.loadingLeft > 0) // NOTE: computedLoadingTime is never 0, so this works
	{
		--ww.loadingLeft;
		if(ww.loadingLeft <= 0)
		{
			if(w.playReloadSound)
				game.soundPlayer->play(24);
				
			ww.available = true;
		}
	}
	
	if(fireCone >= 0)
	{
		--fireCone;
		if(fireCone == 0)
			fireConeActive = false;
	}
	
	if(leaveShellTimer > 0)
	{
		if(--leaveShellTimer <= 0)
		{
			common.nobjectTypes[7].create1(game, game.rand(16000) - 8000, -int(game.rand(20000)), x, y, 0, this);
		}
	}
}

void Worm::processMovement()
{
	Common& common = *game.common;
	
	if(movable)
	{
		bool left = pressed(Left);
		bool right = pressed(Right);
		
		if(left && !right)
		{
			if(velX > common.C[MaxVelLeft])
				velX -= common.C[WalkVelLeft];
				
			if(direction != 0)
			{
				aimingSpeed = 0;
				if(aimingAngle >= itof(64))
					aimingAngle = itof(128) - aimingAngle;
				direction = 0;
			}
			
			animate = true;
		}
		
		if(!left && right)
		{
			if(velX < common.C[MaxVelRight])
				velX += common.C[WalkVelRight];
				
			if(direction != 1)
			{
				aimingSpeed = 0;
				if(aimingAngle <= itof(64))
					aimingAngle = itof(128) - aimingAngle;
				direction = 1;
			}
			
			animate = true;
		}
		
		if(left && right)
		{
			if(ableToDig)
			{
				ableToDig = false;
				
				fixed dirX = cosTable[ftoi(aimingAngle)];
				fixed dirY = sinTable[ftoi(aimingAngle)];
				
				fixed posX = dirX * 2 + x;
				fixed posY = dirY * 2 + y;

				/* TODO
				long iDigx = ftoi(fTempx) - 4;
				if(iDigx < 0)    iDigx = 0;
				if(iDigx >= levwidth) iDigx = levwidth-1;

				long iDigenx = ftoi(fTempx) + 4;
				if(iDigenx < 0)    iDigenx = 0;
				if(iDigenx >= levwidth) iDigenx = levwidth-1;

				long iDigy;

				long iDigsty = ftoi(fTempy) - 4;
				if(iDigsty < 0)    iDigsty = 0;
				if(iDigsty >= levheight) iDigsty = levheight-1;

				long iDigeny = ftoi(fTempy) + 4;
				if(iDigeny < 0)    iDigeny = 0;
				if(iDigeny >= levheight) iDigeny = levheight-1;

				for(; iDigx <= iDigenx; iDigx++)
				{
					for(iDigy = iDigsty; iDigy <= iDigeny; iDigy++)
					{
						//Throw away every third pixel
						if(materials.Dirt[lev(iDigx, iDigy)] && random(3) == 0)
						{
							CreateObject2(random(128), 0, 0, itof(iDigx), itof(iDigy), lev(iDigx, iDigy), 2, BYTE(w));
						} // 419A
					} // 41A9
				} // 41BB
*/

				posX -= itof(7);
				posY -= itof(7);
				
				int ix = ftoi(posX), iy = ftoi(posY);
				drawDirtEffect(common, game.rand, game.level, 7, ix, iy);
				if(game.settings->shadow)
					correctShadow(common, game.level, Rect(ix - 3, iy - 3, ix + 18, iy + 18));
				
				posX += dirX << 1;
				posY += dirY << 1;

//l_43EB:
				ix = ftoi(posX);
				iy = ftoi(posY);
				drawDirtEffect(common, game.rand, game.level, 7, ix, iy);
				if(game.settings->shadow)
					correctShadow(common, game.level, Rect(ix - 3, iy - 3, ix + 18, iy + 18));
				
				//NOTE! Maybe the shadow corrections can be joined into one? Mmm?
			} // 4552
		}
		else
		{
			ableToDig = true;
		}
		
		if(!left && !right)
		{
			animate = false; //Don't animate the this unless he is moving
		} // 458C
	}
}

void Worm::processTasks()
{
	Common& common = *game.common;
	
	if(pressed(Change))
	{
		if(ninjarope.out)
		{
			if(pressed(Up))
				ninjarope.length -= common.C[NRPullVel]; 
			if(pressed(Down))
				ninjarope.length += common.C[NRReleaseVel];
				
			if(ninjarope.length < common.C[NRMinLength])
				ninjarope.length = common.C[NRMinLength];
			if(ninjarope.length > common.C[NRMaxLength])
				ninjarope.length = common.C[NRMaxLength];
		}
		
		if(pressedOnce(Jump))
		{
			ninjarope.out = true;
			ninjarope.attached = false;
			
			game.soundPlayer->play(5);
			
			ninjarope.x = x;
			ninjarope.y = y;
			
			ninjarope.velX = cosTable[ftoi(aimingAngle)] << common.C[NRThrowVelX];
			ninjarope.velY = sinTable[ftoi(aimingAngle)] << common.C[NRThrowVelY];
									
			ninjarope.length = common.C[NRInitialLength];
		}
	}
	else
	{
		//Jump = remove ninjarope, jump
		if(pressed(Jump))
		{
			ninjarope.out = false;
			ninjarope.attached = false;
			
			if((reacts[RFUp] > 0 || common.H[HAirJump])
			&& (ableToJump || common.H[HMultiJump]))
			{
				velY -= common.C[JumpForce];
				ableToJump = false;
			}
		}
		else
			ableToJump = true;
	}
}

void Worm::processAiming()
{
	Common& common = *game.common;
	
	bool up = pressed(Up);
	bool down = pressed(Down);
	
	if(aimingSpeed != 0)
	{
		aimingAngle += aimingSpeed;
				
		if(!up && !down)
		{
			aimingSpeed = (aimingSpeed * common.C[AimFricMult]) / common.C[AimFricDiv];
		}
		
		if(direction == 1)
		{
			if(ftoi(aimingAngle) > common.C[AimMaxRight])
			{
				aimingSpeed = 0;
				aimingAngle = itof(common.C[AimMaxRight]);
			}
			if(ftoi(aimingAngle) < common.C[AimMinRight])
			{
				aimingSpeed = 0;
				aimingAngle = itof(common.C[AimMinRight]);
			}
		}
		else
		{
			if(ftoi(aimingAngle) < common.C[AimMaxLeft])
			{
				aimingSpeed = 0;
				aimingAngle = itof(common.C[AimMaxLeft]);
			}
			if(ftoi(aimingAngle) > common.C[AimMinLeft])
			{
				aimingSpeed = 0;
				aimingAngle = itof(common.C[AimMinLeft]);
			}
		}
	}
	
	if(movable && (!ninjarope.out || !pressed(Change)))
	{
		if(up)
		{
			if(direction == 0)
			{
				if(aimingSpeed < common.C[MaxAimVelLeft])
					aimingSpeed += common.C[AimAccLeft];
			}
			else
			{
				if(aimingSpeed > common.C[MaxAimVelRight])
					aimingSpeed -= common.C[AimAccRight];
			}
		}
		
		if(down)
		{
			if(direction == 1)
			{
				if(aimingSpeed < common.C[MaxAimVelLeft])
					aimingSpeed += common.C[AimAccLeft];
			}
			else
			{
				if(aimingSpeed > common.C[MaxAimVelRight])
					aimingSpeed -= common.C[AimAccRight];
			}
		}
	}
}

void Worm::processWeaponChange()
{
	Common& common = *game.common;
	
	if(!keyChangePressed)
	{
		release(Left);
		release(Right);
		
		keyChangePressed = true;
	}
	
	fireConeActive = 0;
	animate = false;
	
	if(common.weapons[weapons[currentWeapon].id].loopSound)
	{
		game.soundPlayer->stop(&weapons[currentWeapon]);
	}
	
	if(weapons[currentWeapon].available || game.settings->loadChange)
	{
		if(pressedOnce(Left))
		{
			if(--currentWeapon < 0)
				currentWeapon = game.settings->selectableWeapons - 1;
				
			hotspotX = ftoi(x);
			hotspotY = ftoi(y);
		}
		
		if(pressedOnce(Right))
		{
			if(++currentWeapon >= game.settings->selectableWeapons)
				currentWeapon = 0;
				
			hotspotX = ftoi(x);
			hotspotY = ftoi(y);
		}
	}
}

void Worm::fire()
{
	Common& common = *game.common;
	WormWeapon& ww = weapons[currentWeapon];
	Weapon& w = common.weapons[ww.id];
	
	--ww.ammo;
	ww.delayLeft = w.delay;
	
	fireCone = w.fireCone;
	if(fireCone)
		fireConeActive = true; // TODO: Consider removing fireConeActive since fireCone seems to imply it's state
		
	fixed firingX = cosTable[ftoi(aimingAngle)] * (w.detectDistance + 5) + x;
	fixed firingY = sinTable[ftoi(aimingAngle)] * (w.detectDistance + 5) + y - itof(1);
	
	if(w.leaveShells > 0)
	{
		if(game.rand(w.leaveShells) == 0)
		{
			leaveShellTimer = w.leaveShellDelay;
		}
	}
	
	if(w.launchSound >= 0)
	{
		if(w.loopSound)
		{
			if(!game.soundPlayer->isPlaying(&weapons[currentWeapon]))
			{
				game.soundPlayer->play(w.launchSound, &weapons[currentWeapon], -1);
			}
			/* TODO
			if(FSOUND_IsPlaying(weapsettings.launchsound[this->weapons[this->currentweapon].id]))
			{
				playsound(
					weapsettings.loopsound[this->weapons[this->currentweapon].id],
					weapsettings.launchsound[this->weapons[this->currentweapon].id],
					soundpointers[weapsettings.launchsound[this->weapons[this->currentweapon].id]]
				);
			}
			*/
		}
		else
		{
			game.soundPlayer->play(w.launchSound);
		}
	}
		
	if(w.affectByWorm)
	{
		int speed = w.speed;
		if(speed < 100)
			speed = 100;
		int parts = w.parts;
		
		if(parts > 0)
		{
			fixed firingVelX = velX * 100 / speed;
			fixed firingVelY = velY * 100 / speed;
			
			for(int i = 0; i < parts; ++i)
			{
				w.fire(
					game,
					ftoi(aimingAngle),
					firingVelX,
					firingVelY,
					speed,
					firingX,
					firingY,
					this);
			}
		}
	}
	else
	{
		int parts = w.parts;
		
		if(parts > 0)
		{
			for(int i = 0; i < parts; ++i)
			{
				w.fire(
					game,
					ftoi(aimingAngle),
					0,
					0,
					w.speed,
					firingX,
					firingY,
					this);
			}
		}
	}
	
	int recoil = w.recoil;
	
	if(common.H[HSignedRecoil] && recoil >= 128)
		recoil -= 256;
	
	velX -= (cosTable[ftoi(aimingAngle)] * recoil) / 100;
	velY -= (sinTable[ftoi(aimingAngle)] * recoil) / 100;
}

bool checkForWormHit(int x, int y, int dist, Worm* ownWorm)
{
	Game& game = ownWorm->game;
	
	for(std::size_t i = 0; i < game.worms.size(); ++i)
	{
		Worm& w = *game.worms[i];
		
		if(&w != ownWorm)
		{
			return checkForSpecWormHit(x, y, dist, w);
		}
	}
	
	return false;
}

bool checkForSpecWormHit(int x, int y, int dist, Worm& w)
{
	Game& game = w.game;
	Common& common = *game.common;
	
	if(!w.visible)
		return false;
		
	PalIdx* wormSprite = common.wormSprite(w.currentFrame, w.direction, 0);
			
	int deltaX = x - ftoi(w.x) + 7;
	int deltaY = y - ftoi(w.y) + 5;
	
	Rect r(deltaX - dist, deltaY - dist, deltaX + dist + 1, deltaY + dist + 1);
	
	r.intersect(Rect(0, 0, 16, 16));
	
	for(int cy = r.y1; cy < r.y2; ++cy)
	for(int cx = r.x1; cx < r.x2; ++cx)
	{
		if(common.materials[wormSprite[cy*16 + cx]].worm())
			return true;
	}
	
	return false;
}

void Worm::processSight()
{
	Common& common = *game.common;
	
	WormWeapon& ww = weapons[currentWeapon];
	Weapon& w = common.weapons[ww.id];
	
	if(ww.available
	&& (w.laserSight || ww.id == common.C[LaserWeapon] - 1))
	{
		fixed dirX = cosTable[ftoi(aimingAngle)];
		fixed dirY = sinTable[ftoi(aimingAngle)];
		fixed tempX = x + dirX * 6;
		fixed tempY = y + dirY * 6 - itof(1);
		
		do
		{
			tempX += dirX;
			tempY += dirY;
			makeSightGreen = checkForWormHit(ftoi(tempX), ftoi(tempY), 0, this);
		}
		while(
			tempX >= 0 &&
			tempY >= 0 &&
			tempX < itof(game.level.width) &&
			tempY < itof(game.level.height) &&
			common.materials[game.level.pixel(ftoi(tempX), ftoi(tempY))].background() &&
			!makeSightGreen);
			
		hotspotX = ftoi(tempX);
		hotspotY = ftoi(tempY);
	}
	else
		makeSightGreen = false;
}

void Worm::processSteerables()
{
	Common& common = *game.common;
	
	WormWeapon& ww = weapons[currentWeapon];
	if(common.weapons[ww.id].shotType == Weapon::STSteerable)
	{
		for(Game::WObjectList::iterator i = game.wobjects.begin(); i != game.wobjects.end(); ++i)
		{
			if(i->id == ww.id && i->owner == this)
			{
				if(pressed(Left))
					i->curFrame -= (game.cycles & 1) + 1;
					
				if(pressed(Right))
					i->curFrame += (game.cycles & 1) + 1;
					
				i->curFrame &= 127; // Wrap
				movable = false;
			}
		}
	}
}
