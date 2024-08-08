#include "worm.hpp"
#include "game.hpp"
#include "mixer/player.hpp"
#include "gfx/renderer.hpp"
#include "constants.hpp"
#include "console.hpp"
#include "filesystem.hpp" // For joinPath
#include <cstdlib>

#include <gvl/serialization/context.hpp>
#include <gvl/serialization/archive.hpp>
#include "replay.hpp"

#include <gvl/crypt/gash.hpp>
#include <gvl/io2/fstream.hpp>

struct Point
{
	int x, y;
};

gvl::gash::value_type& WormSettings::updateHash()
{
	GameSerializationContext context;
	gvl::hash_accumulator<gvl::gash> ha;


	archive(gvl::out_archive<gvl::hash_accumulator<gvl::gash>, GameSerializationContext>(ha, context), *this);

	ha.flush();
	hash = ha.final();
	return hash;
}

void WormSettings::saveProfile(FsNode node)
{
	try
	{
		//auto const& fullPath = path + ".lpf";
		//create_directories(fullPath);
		//gvl::sink str(new gvl::file_bucket_pipe(fullPath.c_str(), "wb"));

		//gvl::octet_writer writer(str);

		auto writer = node.toOctetWriter();

		//profilePath = path;
		profileNode = node;
		GameSerializationContext context;
		archive(gvl::out_archive<gvl::octet_writer, GameSerializationContext>(writer, context), *this);
	}
	catch(gvl::stream_error& e)
	{
		Console::writeWarning(std::string("Stream error saving profile: ") + e.what());
	}
}

void WormSettings::loadProfile(FsNode node)
{
	int oldColor = color;
	try
	{
		//gvl::source str(gvl::to_source(new gvl::file_bucket_pipe(path.c_str(), "rb")));

		//gvl::octet_reader reader(str);

		auto reader = node.toOctetReader();

		profileNode = node;
		GameSerializationContext context;
		archive(gvl::in_archive<gvl::octet_reader, GameSerializationContext>(reader, context), *this);
	}
	catch(gvl::stream_error& e)
	{
		Console::writeWarning(std::string("Stream error loading profile: ") + e.what());
		Console::writeWarning("The profile may just be old, in which case there is nothing to worry about");
	}

	color = oldColor;  // We preserve the color
}

void Worm::calculateReactionForce(Game& game, int newX, int newY, int dir)
{
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

		if(!game.level.checkedMatWrap(colX, colY).background())
		{
			++reacts[dir];
		}
	}
}

void Worm::processPhysics(Game& game)
{
	Common& common = *game.common;

	if(reacts[RFUp] > 0)
		vel.x = (vel.x * LC(WormFricMult)) / LC(WormFricDiv);

	fixedvec absvel(std::abs(vel.x), std::abs(vel.y));

	int32 rh, rv, mbh, mbv;

	rh = reacts[vel.x >= 0 ? RFLeft : RFRight];
	rv = reacts[vel.y >= 0 ? RFUp : RFDown];
	mbh = vel.x > 0 ? LC(MinBounceRight) : -LC(MinBounceLeft);
	mbv = vel.y > 0 ? LC(MinBounceDown) : -LC(MinBounceUp);

	if (vel.x && rh) // TODO: We wouldn't need the vel.x check if we knew that mbh/mbv were always non-zero
	{
		if(absvel.x > mbh)
		{
			if(common.H[HFallDamage])
				health -= LC(FallDamageRight);
			else
				game.soundPlayer->play(14);
			vel.x = -vel.x / 3;
		}
		else
			vel.x = 0;
	}

	if(vel.y && rv)
	{
		if(absvel.y > mbv)
		{
			if(common.H[HFallDamage])
				health -= LC(FallDamageDown);
			else
				game.soundPlayer->play(14);
			vel.y = -vel.y / 3;
		}
		else
			vel.y = 0;
	}

	if(reacts[RFUp] == 0)
	{
		vel.y += LC(WormGravity);
	}

	// No, we can't use rh/rv here, they are out of date
	if(reacts[vel.x >= 0 ? RFLeft : RFRight] < 2)
		pos.x += vel.x;

	if(reacts[vel.y >= 0 ? RFUp : RFDown] < 2)
		pos.y += vel.y;
}

void Worm::process(Game& game)
{
	Common& common = *game.common;

	if(health > settings->health)
		health = settings->health;

	if((game.settings->gameMode != Settings::GMKillEmAll && game.settings->gameMode != Settings::GMScalesOfJustice)
	|| lives > 0)
	{
		if(visible)
		{
			// Liero.exe: 291C

			auto next = pos + vel;
			auto iNext = ftoi(next);

			{ // Calculate reaction forces

				for(int i = 0; i < 4; i++)
				{
					calculateReactionForce(game, iNext.x, iNext.y, i);

					// Yes, Liero does this in every iteration. Keep it this way.


					if(iNext.x < 4)
					{
						reacts[RFRight] += 5;
					}
					else if(iNext.x > game.level.width - 5)
					{
						reacts[RFLeft] += 5;
					}

					if(iNext.y < 5)
					{
						reacts[RFDown] += 5;
					}
					else
					{
						if(common.H[HWormFloat])
						{
							if(iNext.y > LC(WormFloatLevel))
								vel.y -= LC(WormFloatPower);
						}
						else if(iNext.y > game.level.height - 6)
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

							pos.y -= itof(1);
							next.y = pos.y + vel.y;
							iNext.y = ftoi(next.y);

							calculateReactionForce(game, iNext.x, iNext.y, RFLeft);
							calculateReactionForce(game, iNext.x, iNext.y, RFRight);
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

							pos.y += itof(1);
							next.y = pos.y + vel.y;
							iNext.y = ftoi(next.y);

							calculateReactionForce(game, iNext.x, iNext.y, RFLeft);
							calculateReactionForce(game, iNext.x, iNext.y, RFRight);
						}
					}
				}
			}

			auto ipos = ftoi(pos);

			auto br = game.bonuses.all();
			for (Bonus* i; (i = br.next()); )
			{
				if(ipos.x + 5 > ftoi(i->x)
				&& ipos.x - 5 < ftoi(i->x)
				&& ipos.y + 5 > ftoi(i->y)
				&& ipos.y - 5 < ftoi(i->y))
				{
					if(i->frame == 1)
					{
						if(health < settings->health)
						{
							game.bonuses.free(br);

							game.doHealing(*this, (game.rand(LC(BonusHealthVar)) + LC(BonusMinHealth)) * settings->health / 100);

						}
					}
					else if(i->frame == 0)
					{
						if(game.rand(LC(BonusExplodeRisk)) > 1)
						{
							WormWeapon& ww = weapons[currentWeapon];

							if(!common.H[HBonusReloadOnly])
							{
								fireCone = 0;

								ww.type = &common.weapons[i->weapon];
								ww.ammo = ww.type->ammo;
							}

							game.soundPlayer->play(24);

							game.bonuses.free(br);

							ww.loadingLeft = 0;
						}
						else
						{
							int bix = ftoi(i->x);
							int biy = ftoi(i->y);
							game.bonuses.free(br);
							common.sobjectTypes[0].create(game, bix, biy, index, 0);
						}
					}
				}
			}

			processSteerables(game);

			if(!movable && !pressed(Left) && !pressed(Right)) // processSteerables sets movable to false, does this interfer?
			{
				movable = true;
			} // 2FB1

			processAiming(game);
			processTasks(game);
			processWeapons(game);

			if(pressed(Fire) && !pressed(Change)
			&& weapons[currentWeapon].available()
			&& weapons[currentWeapon].delayLeft <= 0)
			{
				fire(game);
			}
			else
			{
				if(weapons[currentWeapon].type->loopSound)
					game.soundPlayer->stop(&weapons[currentWeapon]);
			}

			processPhysics(game);
			processSight(game);

			if(pressed(Change))
			{
				processWeaponChange(game);
			}
			else
			{
				keyChangePressed = false;
				processMovement(game);
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

					common.nobjectTypes[6].create1(game, vel, pos, 0, index, 0);
				}
			}

			if(health <= 0)
			{
				leaveShellTimer = 0;
				makeSightGreen = false;

				Weapon const& w = *weapons[currentWeapon].type;
				if(w.loopSound)
				{
					game.soundPlayer->stop(&weapons[currentWeapon]);
				}

				int deathSnd = 15 + game.rand(3);
				game.soundPlayer->play(deathSnd, this);

				fireCone = 0;
				ninjarope.out = false;

				if (game.settings->gameMode == Settings::GMScalesOfJustice)
				{
					while (health <= 0)
					{
						health += settings->health;
						--lives;
					}
				}
				else
				{
					--lives;
				}

				int oldLastKilled = game.lastKilledIdx;
				// For GameOfTag, 'it' doesn't change if the killer
				// was not 'it', itself, unknown or there were no 'it'.
				if(game.settings->gameMode != Settings::GMGameOfTag
				|| game.lastKilledIdx < 0
				|| lastKilledByIdx < 0
				|| lastKilledByIdx == index
				|| lastKilledByIdx == game.lastKilledIdx)
				{
					game.lastKilledIdx = index;
				}
				game.gotChanged = (oldLastKilled != game.lastKilledIdx);

				if(lastKilledByIdx >= 0 && lastKilledByIdx != index)
				{
					++game.wormByIdx(lastKilledByIdx)->kills;
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
							vel / 3,
							pos,
							0,
							index, 0);
					}
				}

				for(int i = 7; i <= 105; i += 14)
				{
					common.nobjectTypes[index].create2(
						game,
						i + game.rand(14),
						vel / 3,
						pos,
						0,
						index, 0);
				}

				game.statsRecorder->afterDeath(this);

				release(Fire);
			}

			// Update frame
			int animFrame = animate ? ((game.cycles & 31) >> 3) : 0;
			currentFrame = angleFrame() + game.settings->wormAnimTab[animFrame];
		}
		else
		{
			// Worm is dead
			steerableCount = 0;

			if(pressedOnce(Fire))
				ready = true;

			if(killedTimer > 0)
				--killedTimer;

			if(killedTimer == 0 && !game.quickSim) // Don't respawn in quicksim
				beginRespawn(game);

			if(killedTimer < 0)
				doRespawning(game);
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

void DumbLieroAI::process(Game& game, Worm& worm)
{
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
			int len = sqrVectorLength(ftoi(worm.pos.x) - ftoi(w->pos.x), ftoi(worm.pos.y) - ftoi(w->pos.y));
			if(!target || len < minLen) // First or closer worm
			{
				target = w;
				minLen = len;
			}
		}
	}

	int maxDist;

	WormWeapon& ww = worm.weapons[worm.currentWeapon];
	Weapon const& w = *ww.type;

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

	fixedvec delta = target->pos - worm.pos;
	auto idelta = ftoi(delta);

	int realDist = vectorLength(idelta.x, idelta.y);

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
		delta /= realDist;
	}
	else
	{
		delta.zero();
	} // 4F2F

	int dir = 1;

	for(; dir < 128; ++dir)
	{
		if(std::abs(cossinTable[dir].x - delta.x) < 0xC00
		&& std::abs(cossinTable[dir].y - delta.y) < 0xC00) // The original had 0xC000, which is wrong
			break;
	} // 4F93

	fixed adeltaX = std::abs(delta.x);
	fixed adeltaY = std::abs(delta.y);

	if(dir >= 128)
	{
		if(delta.x > 0)
		{
			if(delta.y < 0)
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
			if(delta.y < 0)
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
			worm.setControlState(Worm::Right, (delta.x > 0));
			worm.setControlState(Worm::Left, (delta.x <= 0));
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

void Worm::initWeapons(Game& game)
{
	Common& common = *game.common;
	currentWeapon = 0; // It was 1 in OpenLiero A1

	for(int j = 0; j < Settings::selectableWeapons; ++j)
	{
		WormWeapon& ww = weapons[j];
		ww.type = &common.weapons[common.weapOrder[settings->weapons[j] - 1]];
		ww.ammo = ww.type->ammo;
		ww.delayLeft = 0;
		ww.loadingLeft = 0;
	}
}

void Worm::beginRespawn(Game& game)
{
	Common& common = *game.common;

	auto temp = ftoi(pos);

	logicRespawn = temp - gvl::ivec2(80, 80);

	auto enemy = temp;

	if(game.worms.size() == 2)
	{
		enemy = ftoi(game.worms[index ^ 1]->pos);
	}

	int trials = 0;
	do
	{
		pos.x = itof(LC(WormSpawnRectX) + game.rand(LC(WormSpawnRectW)));
		pos.y = itof(LC(WormSpawnRectY) + game.rand(LC(WormSpawnRectH)));

		// The original didn't have + 4 in both, which seems
		// to be done in the exe and makes sense.
		while(ftoi(pos.y) + 4 < game.level.height
		&& game.level.mat(ftoi(pos.x), ftoi(pos.y) + 4).background())
		{
			pos.y += itof(1);
		}

		if(++trials >= 50000)
			break;
	}
	while(!checkRespawnPosition(game, enemy.x, enemy.y, temp.x, temp.y, ftoi(pos.x), ftoi(pos.y)));

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

void Worm::doRespawning(Game& game)
{
	Common& common = *game.common;

	for(int c = 0; c < 4; c++)
	{
		if(logicRespawn.x < ftoi(pos.x) - 80) ++logicRespawn.x;
		else if(logicRespawn.x > ftoi(pos.x) - 80) --logicRespawn.x;

		if(logicRespawn.y < ftoi(pos.y) - 80) ++logicRespawn.y;
		else if(logicRespawn.y > ftoi(pos.y) - 80) --logicRespawn.y;
	}

	limitXY(logicRespawn.x, logicRespawn.y, game.level.width - 158, game.level.height - 158);

	int destX = ftoi(pos.x) - 80;
	int destY = ftoi(pos.y) - 80;
	limitXY(destX, destY, game.level.width - 158, game.level.height - 158);

	if(logicRespawn.x < destX + 5
	&& logicRespawn.x > destX - 5
	&& logicRespawn.y < destY + 5
	&& logicRespawn.y > destY - 5
	&& ready) // Don't spawn in quicksim
	{
		auto ipos = ftoi(pos);
		drawDirtEffect(common, game.rand, game.level, 0, ipos.x - 7, ipos.y - 7);
		if(game.settings->shadow)
			correctShadow(common, game.level, gvl::rect(ipos.x - 10, ipos.y - 10, ipos.x + 11, ipos.y + 11));

		ready = false;
		game.soundPlayer->play(21);

		visible = true;
		fireCone = 0;
		vel.zero();
		if (game.settings->gameMode != Settings::GMScalesOfJustice)
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

		game.statsRecorder->afterSpawn(this);
	}
}

void Worm::processWeapons(Game& game)
{
	Common& common = *game.common;

	for(int i = 0; i < Settings::selectableWeapons; ++i)
	{
		if(weapons[i].delayLeft >= 0)
			--weapons[i].delayLeft;
	}

	WormWeapon& ww = weapons[currentWeapon];
	Weapon const& w = *ww.type;

	if(ww.ammo <= 0)
	{
		int computedLoadingTime = w.computedLoadingTime(*game.settings);
		ww.loadingLeft = computedLoadingTime;
		ww.ammo = w.ammo;
	}

	if(ww.loadingLeft > 0) // NOTE: computedLoadingTime is never 0, so this works
	{
		--ww.loadingLeft;
		if(ww.loadingLeft <= 0 && w.playReloadSound)
		{
			game.soundPlayer->play(24);
		}
	}

	if(fireCone > 0)
	{
		--fireCone;
	}

	if(leaveShellTimer > 0)
	{
		if(--leaveShellTimer <= 0)
		{
			auto velY = -int(game.rand(20000));
			auto velX = game.rand(16000) - 8000;
			common.nobjectTypes[7].create1(game, fixedvec(velX, velY), pos, 0, index, 0);
		}
	}
}

void Worm::processMovement(Game& game)
{
	Common& common = *game.common;

	if(movable)
	{
		bool left = pressed(Left);
		bool right = pressed(Right);

		if(left && !right)
		{
			if(vel.x > LC(MaxVelLeft))
				vel.x -= LC(WalkVelLeft);

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
			if(vel.x < LC(MaxVelRight))
				vel.x += LC(WalkVelRight);

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

				fixedvec dir(cossinTable[ftoi(aimingAngle)]);

				auto digPos = dir * 2 + pos;

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

				digPos.x -= itof(7);
				digPos.y -= itof(7);

				auto idigPos = ftoi(digPos);
				drawDirtEffect(common, game.rand, game.level, 7, idigPos.x, idigPos.y);
				if(game.settings->shadow)
					correctShadow(common, game.level, gvl::rect(idigPos.x - 3, idigPos.y - 3, idigPos.x + 18, idigPos.y + 18));

				digPos += dir * 2;

//l_43EB:
				idigPos = ftoi(digPos);
				drawDirtEffect(common, game.rand, game.level, 7, idigPos.x, idigPos.y);
				if(game.settings->shadow)
					correctShadow(common, game.level, gvl::rect(idigPos.x - 3, idigPos.y - 3, idigPos.x + 18, idigPos.y + 18));

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

void Worm::processTasks(Game& game)
{
	Common& common = *game.common;

	if(pressed(Change))
	{
		if(ninjarope.out)
		{
			if(pressed(Up))
				ninjarope.length -= LC(NRPullVel);
			if(pressed(Down))
				ninjarope.length += LC(NRReleaseVel);

			if(ninjarope.length < LC(NRMinLength))
				ninjarope.length = LC(NRMinLength);
			if(ninjarope.length > LC(NRMaxLength))
				ninjarope.length = LC(NRMaxLength);
		}

		if(pressedOnce(Jump))
		{
			ninjarope.out = true;
			ninjarope.attached = false;

			game.soundPlayer->play(5);

			ninjarope.pos = pos;
			ninjarope.vel = fixedvec(cossinTable[ftoi(aimingAngle)].x << LC(NRThrowVelX), cossinTable[ftoi(aimingAngle)].y << LC(NRThrowVelY));

			ninjarope.length = LC(NRInitialLength);
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
				vel.y -= LC(JumpForce);
				ableToJump = false;
			}
		}
		else
			ableToJump = true;
	}
}

void Worm::processAiming(Game& game)
{
	Common& common = *game.common;

	bool up = pressed(Up);
	bool down = pressed(Down);

	if(aimingSpeed != 0)
	{
		aimingAngle += aimingSpeed;

		if(!up && !down)
		{
			aimingSpeed = (aimingSpeed * LC(AimFricMult)) / LC(AimFricDiv);
		}

		if(direction == 1)
		{
			if(ftoi(aimingAngle) > LC(AimMaxRight))
			{
				aimingSpeed = 0;
				aimingAngle = itof(LC(AimMaxRight));
			}
			if(ftoi(aimingAngle) < LC(AimMinRight))
			{
				aimingSpeed = 0;
				aimingAngle = itof(LC(AimMinRight));
			}
		}
		else
		{
			if(ftoi(aimingAngle) < LC(AimMaxLeft))
			{
				aimingSpeed = 0;
				aimingAngle = itof(LC(AimMaxLeft));
			}
			if(ftoi(aimingAngle) > LC(AimMinLeft))
			{
				aimingSpeed = 0;
				aimingAngle = itof(LC(AimMinLeft));
			}
		}
	}

	if(movable && (!ninjarope.out || !pressed(Change)))
	{
		if(up)
		{
			if(direction == 0)
			{
				if(aimingSpeed < LC(MaxAimVelLeft))
					aimingSpeed += LC(AimAccLeft);
			}
			else
			{
				if(aimingSpeed > LC(MaxAimVelRight))
					aimingSpeed -= LC(AimAccRight);
			}
		}

		if(down)
		{
			if(direction == 1)
			{
				if(aimingSpeed < LC(MaxAimVelLeft))
					aimingSpeed += LC(AimAccLeft);
			}
			else
			{
				if(aimingSpeed > LC(MaxAimVelRight))
					aimingSpeed -= LC(AimAccRight);
			}
		}
	}
}

void Worm::processWeaponChange(Game& game)
{
	if(!keyChangePressed)
	{
		release(Left);
		release(Right);

		keyChangePressed = true;
	}

	fireCone = 0;
	animate = false;

	if(weapons[currentWeapon].type->loopSound)
	{
		game.soundPlayer->stop(&weapons[currentWeapon]);
	}

	if(weapons[currentWeapon].available() || game.settings->loadChange)
	{
		if(pressedOnce(Left))
		{
			if(--currentWeapon < 0)
				currentWeapon = Settings::selectableWeapons - 1;

			hotspotX = ftoi(pos.x);
			hotspotY = ftoi(pos.y);
		}

		if(pressedOnce(Right))
		{
			if(++currentWeapon >= Settings::selectableWeapons)
				currentWeapon = 0;

			hotspotX = ftoi(pos.x);
			hotspotY = ftoi(pos.y);
		}
	}
}

void Worm::fire(Game& game)
{
	Common& common = *game.common;
	WormWeapon& ww = weapons[currentWeapon];
	Weapon const& w = *ww.type;

	--ww.ammo;
	ww.delayLeft = w.delay;

	fireCone = w.fireCone;

	fixedvec firing(
		cossinTable[ftoi(aimingAngle)] * (w.detectDistance + 5) + pos - fixedvec(0, itof(1)));

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
		}
		else
		{
			game.soundPlayer->play(w.launchSound);
		}
	}

	int speed = w.speed;
	fixedvec firingVel;
	int parts = w.parts;

	if(w.affectByWorm)
	{
		if(speed < 100)
			speed = 100;

		firingVel = vel * 100 / speed;
	}

	for(int i = 0; i < parts; ++i)
	{
		w.fire(
			game,
			ftoi(aimingAngle),
			firingVel,
			speed,
			firing,
			index, &ww);
	}

	int recoil = w.recoil;

	if(common.H[HSignedRecoil] && recoil >= 128)
		recoil -= 256;

	vel -= cossinTable[ftoi(aimingAngle)] * recoil / 100;
}

bool checkForWormHit(Game& game, int x, int y, int dist, Worm* ownWorm)
{
	for(std::size_t i = 0; i < game.worms.size(); ++i)
	{
		Worm& w = *game.worms[i];

		if(&w != ownWorm)
		{
			return checkForSpecWormHit(game, x, y, dist, w);
		}
	}

	return false;
}

bool checkForSpecWormHit(Game& game, int x, int y, int dist, Worm& w)
{
	Common& common = *game.common;

	if(!w.visible)
		return false;

	PalIdx* wormSprite = common.wormSprite(w.currentFrame, w.direction, 0);

	int deltaX = x - ftoi(w.pos.x) + 7;
	int deltaY = y - ftoi(w.pos.y) + 5;

	gvl::rect r(deltaX - dist, deltaY - dist, deltaX + dist + 1, deltaY + dist + 1);

	r.intersect(gvl::rect(0, 0, 16, 16));

	for(int cy = r.y1; cy < r.y2; ++cy)
	for(int cx = r.x1; cx < r.x2; ++cx)
	{
		assert(cy*16 + cx < 16*16);
		if(common.materials[wormSprite[cy*16 + cx]].worm())
			return true;
	}

	return false;
}

void Worm::processSight(Game& game)
{
	Common& common = *game.common;

	WormWeapon& ww = weapons[currentWeapon];
	Weapon const& w = *ww.type;

	if(ww.available()
	&& (w.laserSight || ww.type - &common.weapons[0] == LC(LaserWeapon) - 1))
	{
		fixedvec dir = cossinTable[ftoi(aimingAngle)];
		fixedvec temp = fixedvec(pos.x + dir.x * 6, pos.y + dir.y * 6 - itof(1));

		do
		{
			temp += dir;
			makeSightGreen = checkForWormHit(game, ftoi(temp.x), ftoi(temp.y), 0, this);
		}
		while(
			temp.x >= 0 &&
			temp.y >= 0 &&
			temp.x < itof(game.level.width) &&
			temp.y < itof(game.level.height) &&
			game.level.mat(ftoi(temp)).background() &&
			!makeSightGreen);

		hotspotX = ftoi(temp.x);
		hotspotY = ftoi(temp.y);
	}
	else
		makeSightGreen = false;
}

void Worm::processSteerables(Game& game)
{
	steerableCount = 0;
	steerableSumX = 0;
	steerableSumY = 0;

	WormWeapon& ww = weapons[currentWeapon];
	if(ww.type->shotType == Weapon::STSteerable)
	{
		auto wr = game.wobjects.all();
		for (WObject* i; (i = wr.next()); )
		{
			if(i->type == ww.type && i->ownerIdx == index)
			{
				if(pressed(Left))
					i->curFrame -= (game.cycles & 1) + 1;

				if(pressed(Right))
					i->curFrame += (game.cycles & 1) + 1;

				i->curFrame &= 127; // Wrap
				movable = false;

				steerableSumX += ftoi(i->pos.x);
				steerableSumY += ftoi(i->pos.y);
				++steerableCount;
			}
		}
	}
}
