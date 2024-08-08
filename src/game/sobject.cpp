#include "sobject.hpp"

#include "worm.hpp"
#include "game.hpp"
#include "viewport.hpp"
#include "gfx/renderer.hpp"
#include "mixer/player.hpp"
#include "console.hpp"
#include "text.hpp"
#include <cmath>
#include <cstdlib>
#include <cassert>

void SObjectType::create(Game& game, int x, int y, int ownerIdx, WormWeapon* firedBy, WObject* from)
{
	Common& common = *game.common;
	SObject& obj = *game.sobjects.newObjectReuse();

	LTRACE(rand, 0, sobj, game.rand.x);
	LTRACE(sobj, &obj - game.sobjects.arr, cxpo, x);
	LTRACE(sobj, &obj - game.sobjects.arr, cypo, y);

	assert(numSounds < 10);

	if(startSound >= 0)
		game.soundPlayer->play(game.rand(numSounds) + startSound);

	for(std::size_t i = 0; i < game.viewports.size(); ++i)
	{
		Viewport& v = *game.viewports[i];

		if(x > v.x
		&& x < v.x + v.rect.width()
		&& y > v.y
		&& y < v.y + v.rect.height())
		{
			if(itof(shake) > v.shake)
				v.shake = itof(shake);
		}
	}

	obj.id = id;
	obj.x = x - 8;
	obj.y = y - 8;
	obj.curFrame = 0;
	obj.animDelay = animDelay;

	if(flash > game.screenFlash)
	{
		game.screenFlash = flash;
	}

	Worm* owner = game.wormByIdx(ownerIdx);

	game.statsRecorder->damagePotential(owner, firedBy, damage);

	if(damage > 0)
	{
		for(std::size_t i = 0; i < game.worms.size(); ++i)
		{
			Worm& w = *game.worms[i];

			int wix = ftoi(w.pos.x);
			int wiy = ftoi(w.pos.y);

			if(wix < x + detectRange
			&& wix > x - detectRange
			&& wiy < y + detectRange
			&& wiy > y - detectRange)
			{
				int delta = wix - x;
				int power = detectRange - std::abs(delta);
				int powerSum = power;

				if(std::abs(w.vel.x) < itof(2)) // TODO: Read from EXE
				{
					if(delta > 0)
						w.vel.x += blowAway * power;
					else
						w.vel.x -= blowAway * power;
				}

				delta = wiy - y;
				power = detectRange - std::abs(delta);
				powerSum = (powerSum + power) / 2;

				if(std::abs(w.vel.y) < itof(2)) // TODO: Read from EXE
				{
					if(delta > 0)
						w.vel.y += blowAway * power;
					else
						w.vel.y -= blowAway * power;
				}

				int z = damage * powerSum;
				if(detectRange)
					z /= detectRange;

				if (from && !from->hasHit)
				{
					game.statsRecorder->hit(owner, firedBy, &w);
					from->hasHit = true;
				}

				if(w.health > 0)
				{
					game.doDamage(w, z, ownerIdx);
					game.statsRecorder->damageDealt(owner, firedBy, &w, z, false);

					int bloodAmount = game.settings->blood * powerSum / 100;

					if(bloodAmount > 0)
					{
						for(int i = 0; i < bloodAmount; ++i)
						{
							int angle = game.rand(128);
							common.nobjectTypes[6].create2(
								game,
								angle,
								w.vel / 3,
								w.pos,
								0,
								w.index,
								firedBy);
						}
					}

					if(game.rand(3) == 0)
					{
						int snd = 18 + game.rand(3); // NOTE: MUST be outside the unpredictable branch below
						if(!game.soundPlayer->isPlaying(&w))
						{
							game.soundPlayer->play(snd, &w);
						}
					}
				}
			}
		} // for( ... worms ...

		int objBlowAway = blowAway / 3; // TODO: Read from EXE

		auto wr = game.wobjects.all();
		for (WObject* i; (i = wr.next()); )
		{
			Weapon const& weapon = *i->type;

			if(weapon.affectByExplosions)
			{
				auto ipos = ftoi(i->pos);
				if(ipos.x < x + detectRange
				&& ipos.x > x - detectRange
				&& ipos.y < y + detectRange
				&& ipos.y > y - detectRange)
				{
					int delta = ipos.x - x;
					int power = detectRange - std::abs(delta);

					if(power > 0)
					{
						if(delta > 0)
							i->vel.x += objBlowAway * power;
						else if(delta < 0)
							i->vel.x -= objBlowAway * power;
					}

					delta = ipos.y - y;
					power = detectRange - std::abs(delta);

					if(power > 0)
					{
						if(delta > 0)
							i->vel.y += objBlowAway * power;
						else if(delta < 0)
							i->vel.y -= objBlowAway * power;
					}

					if(weapon.chainExplosion)
						i->blowUpObject(game, ownerIdx);
				}
			} // if( ... affectByExplosions ...
		} // for( ... wobjects ...

		auto nr = game.nobjects.all();
		for (NObject* i; (i = nr.next()); )
		{
			NObjectType const& t = *i->type;

			if(t.affectByExplosions)
			{
				auto ipos = ftoi(i->pos);
				if(ipos.x < x + detectRange
				&& ipos.x > x - detectRange
				&& ipos.y < y + detectRange
				&& ipos.y > y - detectRange)
				{
					int delta = ipos.x - x;
					int power = detectRange - std::abs(delta);

					if(power > 0)
					{
						if(delta > 0)
							i->vel.x += objBlowAway * power;
						else if(delta < 0)
							i->vel.x -= objBlowAway * power;
					}

					delta = ipos.y - y;
					power = detectRange - std::abs(delta);

					if(power > 0)
					{
						if(delta > 0)
							i->vel.y += objBlowAway * power;
						else if(delta < 0)
							i->vel.y -= objBlowAway * power;
					}

					IF_ENABLE_TRACING(Common& common = *game.common);

					LTRACE(nobj, &*i - game.nobjects.arr, puxp, i->vel.x);
					LTRACE(nobj, &*i - game.nobjects.arr, puyp, i->vel.y);
				}
			}
		}

		{
			int width = detectRange / 2;

			gvl::rect rect(x - width, y - width, x + width + 1, y + width + 1);

			rect.intersect(game.level.rect());

			for(int y = rect.y1; y < rect.y2; ++y)
			for(int x = rect.x1; x < rect.x2; ++x)
			{
				if(game.level.mat(x, y).anyDirt()
				&& game.rand(8) == 0)
				{
					PalIdx pix = game.level.pixel(x, y);
					int angle = game.rand(128);
					common.nobjectTypes[2].create2(
						game,
						angle,
						fixedvec(),
						itof(gvl::ivec2(x, y)),
						pix, ownerIdx, firedBy);
				}
			}
		}

	} // if(damage ...

	if(dirtEffect >= 0)
	{
		drawDirtEffect(common, game.rand, game.level, dirtEffect, x - 7, y - 7);

		if(game.settings->shadow)
			correctShadow(common, game.level, gvl::rect(x - 10, y - 10, x + 11, y + 11));
	}

	auto br = game.bonuses.all();
	for (Bonus* i; (i = br.next()); )
	{
		int ix = ftoi(i->x), iy = ftoi(i->y);

		if(ix > x - detectRange
		&& ix < x + detectRange
		&& iy > y - detectRange
		&& iy < y + detectRange)
		{
			game.bonuses.free(br);
			common.sobjectTypes[0].create(game, ix, iy, ownerIdx, firedBy);
		}
	} // for( ... bonuses ...
}

void SObject::process(Game& game)
{
	Common& common = *game.common;
	SObjectType& t = common.sobjectTypes[id];


	if(--animDelay <= 0)
	{
		animDelay = t.animDelay;
		++curFrame;
		if(curFrame > t.numFrames)
			game.sobjects.free(this);
	}
}
