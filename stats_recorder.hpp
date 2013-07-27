#ifndef STATS_RECORDER_HPP
#define STATS_RECORDER_HPP

#include "worm.hpp"

struct Common;

struct StatsRecorder : gvl::shared
{
	virtual void damagePotential(Worm* byWorm, WormWeapon* weapon, int hp);
	virtual void damageDealt(Worm* byWorm, WormWeapon* weapon, Worm* toWorm, int hp, bool hasHit);
	
	virtual void shot(Worm* byWorm, WormWeapon* weapon);
	virtual void hit(Worm* byWorm, WormWeapon* weapon, Worm* toWorm);

	virtual void afterSpawn(Worm* worm);
	virtual void afterDeath(Worm* worm);
	virtual void tick();
	virtual void finish();

	virtual void write(Common& common, gvl::stream_ptr sink);
};

struct WeaponStats
{
	WeaponStats()
	: potentialHp(0), actualHp(0)
	, potentialHits(0), actualHits(0)
	, totalHp(0)
	{
	}

	int potentialHp, actualHp;
	int potentialHits, actualHits;
	int totalHp;
};

struct WormStats
{
	WormStats()
	: damage(0), damageDealt(0)
	, selfDamage(0), spawnTime(-1)
	{
	}

	std::vector<std::pair<int, int> > lifeSpans;

	void lifeStats(int& min, int& max)
	{
		min = 0;
		max = 0;
		if (lifeSpans.empty())
			return;

		min = lifeSpans[0].second - lifeSpans[0].first;
		max = min;

		for (size_t i = 1; i < lifeSpans.size(); ++i)
		{
			int len = lifeSpans[i].second - lifeSpans[i].first;
			max = std::max(len, max);
			min = std::min(len, min);
		}
	}

	WeaponStats weapons[40];
	int damage, damageDealt, selfDamage;

	int spawnTime;
};

struct NormalStatsRecorder : StatsRecorder
{
	NormalStatsRecorder()
	: frame(0)
	{
	}

	int frame;
	WormStats worms[2];

	void damagePotential(Worm* byWorm, WormWeapon* weapon, int hp);
	void damageDealt(Worm* byWorm, WormWeapon* weapon, Worm* toWorm, int hp, bool hasHit);
	
	void shot(Worm* byWorm, WormWeapon* weapon);
	void hit(Worm* byWorm, WormWeapon* weapon, Worm* toWorm);

	void afterSpawn(Worm* worm);
	void afterDeath(Worm* worm);
	void tick();

	void finish();

	void write(Common& common, gvl::stream_ptr sink);
};

#endif // STATS_RECORDER_HPP
