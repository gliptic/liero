#include "stats_recorder.hpp"

#include <chrono>
#include "common.hpp"
#include "game.hpp"
#include "text.hpp"

void StatsRecorder::damagePotential(Worm* byWorm, WormWeapon* weapon, int hp)
{
}

void StatsRecorder::damageDealt(Worm* byWorm, WormWeapon* weapon, Worm* toWorm, int hp, bool hasHit)
{
}

void StatsRecorder::shot(Worm* byWorm, WormWeapon* weapon)
{
}

void StatsRecorder::hit(Worm* byWorm, WormWeapon* weapon, Worm* toWorm)
{
}

void StatsRecorder::afterSpawn(Worm* worm)
{
}

void StatsRecorder::afterDeath(Worm* worm)
{
}

void StatsRecorder::finish(Game& game)
{

}

void StatsRecorder::preTick(Game& game)
{
}

void StatsRecorder::tick(Game& game)
{

}

void StatsRecorder::aiProcessTime(Worm* worm, std::chrono::nanoseconds time)
{
}

void NormalStatsRecorder::damagePotential(Worm* byWorm, WormWeapon* weapon, int hp)
{
	if (speculative) return;
	if (!byWorm || !weapon)
		return;

	WormStats& ws = worms[byWorm->index];
	WeaponStats& weap = ws.weapons[weapon->type->id];
	weap.potentialHp += hp;
}

void NormalStatsRecorder::damageDealt(Worm* byWorm, WormWeapon* weapon, Worm* toWorm, int hp, bool hasHit)
{
	if (speculative) return;
	assert(toWorm);

	auto& w = worms[toWorm->index];
	w.damage += hp;
	w.wormFrameStats.back().damage += hp;
	w.damageHm.incArea(ftoi(toWorm->pos.x), ftoi(toWorm->pos.y), hp);

	if(byWorm)
	{
		if(byWorm != toWorm)
			worms[byWorm->index].damageDealt += hp;
		else
			worms[byWorm->index].selfDamage += hp;
	}

	if(!byWorm || !weapon)
		return;

	if(byWorm != toWorm) // Don't count if projectile already hit
	{
		WormStats& ws = worms[byWorm->index];
		WeaponStats& weap = ws.weapons[weapon->type->id];
		if (!hasHit)
			weap.actualHp += hp;
		weap.totalHp += hp;
	}
}

void NormalStatsRecorder::shot(Worm* byWorm, WormWeapon* weapon)
{
	if (speculative) return;
	if (!byWorm || !weapon)
		return;

	WormStats& ws = worms[byWorm->index];
	WeaponStats& weap = ws.weapons[weapon->type->id];
	weap.potentialHits += 1;
}

void NormalStatsRecorder::hit(Worm* byWorm, WormWeapon* weapon, Worm* toWorm)
{
	if (speculative) return;
	assert(toWorm);

	if (!byWorm || !weapon)
		return;

	if (byWorm != toWorm)
	{
		WormStats& ws = worms[byWorm->index];
		WeaponStats& weap = ws.weapons[weapon->type->id];
		weap.actualHits += 1;
	}
}

void NormalStatsRecorder::afterSpawn(Worm* worm)
{
	if (speculative) return;
	WormStats& w = worms[worm->index];
	w.spawnTime = frame;
}

void NormalStatsRecorder::afterDeath(Worm* worm)
{
	if (speculative) return;
	WormStats& w = worms[worm->index];
	w.lifeSpans.push_back(std::make_pair(w.spawnTime, frame));
	w.spawnTime = -1;
}


void NormalStatsRecorder::preTick(Game& game)
{
	if (speculative) return;
	frameStart = std::chrono::steady_clock::now();

	for (auto& w : worms)
	{
		w.wormFrameStats.push_back(WormFrameStats());

		Worm& worm = *game.worms[w.index];

		int h = std::max(worm.health, 0);
		if (!worm.visible)
			h = worm.settings->health;

		w.wormFrameStats.back().totalHp = worm.lives * worm.settings->health + h;
	}
}

void NormalStatsRecorder::tick(Game& game)
{
	if (speculative) return;
	auto frameEnd = std::chrono::steady_clock::now();
	processTimeTotal += std::chrono::duration_cast<std::chrono::milliseconds>(frameEnd - frameStart).count();

	for (auto const& w : game.worms)
	{
		auto& ws = worms[w->index];
		if (w->visible)
		{
			presence.inc(ftoi(w->pos.x), ftoi(w->pos.y));
			ws.presence.inc(ftoi(w->pos.x), ftoi(w->pos.y));

			bool ok = true;
			if (!w->controlStates[Worm::Control::Fire]
			 && (!w->controlStates[Worm::Control::Change] || (!w->controlStates[Worm::Control::Left] && !w->controlStates[Worm::Control::Right]))
			 && w->weapons[w->currentWeapon].loadingLeft == 0
			 && std::find_if(w->weapons, w->weapons + 5, [](WormWeapon& ww) { return ww.loadingLeft > 0; }) != w->weapons + 5)
			{
				ok = false;
			}

			ws.weaponChangeGood += ok;
			ws.weaponChangeBad += !ok;
		}
	}

	++frame;
}

void NormalStatsRecorder::finish(Game& game)
{
	for (int i = 0; i < 2; ++i)
	{
		auto const& gw = game.worms[i];
		WormStats& w = worms[i];
		if (w.spawnTime >= 0)
		{
			w.lifeSpans.push_back(std::make_pair(w.spawnTime, frame));
			w.spawnTime = -1;
		}
		w.lives = gw->lives;
		w.timer = gw->timer;
		w.kills = gw->kills;
	}

	gameTime = frame;
}

void NormalStatsRecorder::aiProcessTime(Worm* worm, std::chrono::nanoseconds time)
{
	if (speculative) return;
	WormStats& w = worms[worm->index];
	w.aiProcessTime += time;
}
