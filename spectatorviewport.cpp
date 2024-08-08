#include "spectatorviewport.hpp"
#include "game.hpp"
#include "text.hpp"
#include "math.hpp"
#include "constants.hpp"
#include "gfx/bitmap.hpp"
#include "gfx/renderer.hpp"
#include "gfx/blit.hpp"

void SpectatorViewport::process(Game& game)
{
	int realShake = ftoi(shake);

	// FIXME this is a bit broken
	// TODO: This depends on what is the starting killedTimer
	if (game.wormByIdx(0)->killedTimer == 150 || game.wormByIdx(1)->killedTimer == 150)
	{
		bannerY = -8;
	}

	if (realShake > 0)
	{
		x += rand(realShake * 2) - realShake;
		y += rand(realShake * 2) - realShake;
	}

	if(x < 0) x = 0;
	if(y < 0) y = 0;
	if(x > maxX) x = maxX;
	if(y > maxY) y = maxY;

}

void SpectatorViewport::draw(Game& game, Renderer& renderer, bool isReplay)
{
	Common& common = *game.common;
	int multiplier = renderer.renderResX / 320;
	int centerX = renderer.renderResX / 2;
	gvl::ivec2 renderPos(x, y);
	fixedvec offs = rect.ul() - renderPos;

	for(std::size_t i = 0; i < game.worms.size(); ++i)
	{
		Worm const& worm = *game.worms[i];
		int offsetX = offs.x / (i + 1);
		if (worm.visible)
		{
			int lifebarWidth = worm.health * 100 / worm.settings->health;
			drawBar(renderer.bmp, offsetX + worm.statsX * multiplier, renderer.renderResY - 39, lifebarWidth, lifebarWidth / 10 + 234);
		}
		else
		{
			int lifebarWidth = 100 - (worm.killedTimer * 25) / 37;
			if(lifebarWidth > 0)
			{
				if(lifebarWidth > 100)
					lifebarWidth = 100;
				drawBar(renderer.bmp, offsetX + worm.statsX * multiplier, renderer.renderResY - 39, lifebarWidth, lifebarWidth / 10 + 234);
			}
		}

		// Draw kills status

		WormWeapon const& ww = worm.weapons[worm.currentWeapon];

		if(ww.available())
		{
			if(ww.ammo > 0)
			{
				int ammoBarWidth = ww.ammo * 100 / ww.type->ammo;

				if(ammoBarWidth > 0)
					drawBar(renderer.bmp, offsetX + worm.statsX * multiplier, renderer.renderResY - 34, ammoBarWidth, ammoBarWidth / 10 + 245);
			}
		}
		else
		{
			int ammoBarWidth = 0;

			if(ww.type->loadingTime != 0)
			{
				int computedLoadingTime = ww.type->computedLoadingTime(*game.settings);
				ammoBarWidth = 100 - ww.loadingLeft * 100 / computedLoadingTime;
			}
			else
			{
				ammoBarWidth = 100 - ww.loadingLeft * 100;
			}

			if(ammoBarWidth > 0)
				drawBar(renderer.bmp, offsetX + worm.statsX * multiplier, renderer.renderResY - 34, ammoBarWidth, ammoBarWidth / 10 + 245);

			if((game.cycles % 20) > 10
			&& worm.visible)
			{
				common.font.drawText(renderer.bmp, LS(Reloading), offsetX + worm.statsX * multiplier, 164, 50);
			}
		}

		common.font.drawText(renderer.bmp, (LS(Kills) + toString(worm.kills)), offsetX + worm.statsX * multiplier, renderer.renderResY - 29, 10);

		// always display player names, color and time in spectator view
		common.font.drawText(renderer.bmp, worm.settings->name, offsetX + worm.statsX * multiplier, renderer.renderResY - 15, 7);
		fillRect(renderer.bmp, offsetX + worm.statsX * multiplier - 1, renderer.renderResY - 7 - 1, 8, 8, 7);
		fillRect(renderer.bmp, offsetX + worm.statsX * multiplier, renderer.renderResY - 7, 6, 6, worm.settings->color);
		common.font.drawText(renderer.bmp, timeToStringEx(game.cycles * 14), centerX - 15, renderer.renderResY - 15, 7);

		int const stateColours[2][2] = {{6, 10}, {79, 4}};

		switch(game.settings->gameMode)
		{
		case Settings::GMKillEmAll:
		case Settings::GMScalesOfJustice:
		{
			common.font.drawText(renderer.bmp, (LS(Lives) + toString(worm.lives)), offsetX + worm.statsX * multiplier, renderer.renderResY - 22, 6);
		}
		break;

		case Settings::GMHoldazone:
		{
			int state = 0;

			for (auto* w : game.worms)
				if (w != &worm && w->timer <= worm.timer)
					state = 1;

			int color = stateColours[game.holdazone.holderIdx != worm.index][state];

			common.font.drawText(renderer.bmp, timeToString(worm.timer), 5, 106 + 84 * worm.index, renderer.renderResY - 39, color);
		}
		break;

		case Settings::GMGameOfTag:
		{
			int state = 0;

			for (auto* w : game.worms)
				if (w != &worm && w->timer >= worm.timer)
					state = 1;

			int color = stateColours[game.lastKilledIdx != worm.index][state];

			common.font.drawText(renderer.bmp, timeToString(worm.timer), 5, 106 + 84 * worm.index, renderer.renderResY - 39, color);
		}
		break;
		}
	}

	blitImageNoKeyColour(renderer.bmp, &game.level.data[0], offs.x, offs.y, game.level.width, game.level.height);

	if (game.settings->gameMode == Settings::GMHoldazone)
	{
		bool timingOut = game.holdazone.timeoutLeft < 70 * 4;

		int color = timingOut ? 168 : 50;
		int contenderColor;

		if (game.holdazone.contenderIdx >= 0)
		{
			Worm* contender = game.wormByIdx(game.holdazone.contenderIdx);
			if (timingOut)
				contenderColor = contender->minimapColor();
			else
				contenderColor = Palette::wormColourIndexes[contender->index] + 5;
		}
		else
		{
			contenderColor = color;
		}

		drawDashedLineBox(renderer.bmp,
			game.holdazone.rect.x1 + offs.x,
			game.holdazone.rect.y1 + offs.y,
			color,
			contenderColor,
			game.holdazone.contenderFrames,
			game.settings->zoneCaptureTime,
			game.holdazone.rect.width(),
			game.holdazone.rect.height(), game.cycles / 10);
	}

	for(std::size_t i = 0; i < game.worms.size(); ++i)
	{
		Worm const& worm = *game.worms[i];
		if(!worm.visible
		&& worm.killedTimer <= 0
		&& !worm.ready)
		{
			if (game.settings->allowViewingSpawnPoint && worm.pressed(Worm::Change))
			{
				int tempX = ftoi(worm.pos.x) - 7 + offs.x;
				int tempY = ftoi(worm.pos.y) - 5 + offs.y;

				blitImageTrans(renderer.bmp, common.wormSpriteObj(worm.currentFrame, worm.direction, worm.index), tempX, tempY, game.cycles);
			}
		}

		if(bannerY > -8
		&& worm.health <= 0)
		{
			if(game.settings->gameMode == Settings::GMGameOfTag
			&& game.gotChanged)
			{
				common.font.drawText(renderer.bmp, LS(YoureIt), rect.x1 + 3, bannerY + 1, 0);
				common.font.drawText(renderer.bmp, LS(YoureIt), rect.x1 + 2, bannerY, 50);
			}
		}
	}
	for(std::size_t i = 0; i < game.worms.size(); ++i)
	{
		Worm const& worm = *game.worms[i];
		if (worm.health <= 0 && bannerY > -8)
		{
			if (worm.lastKilledByIdx == worm.index)
			{
				std::string msg(worm.settings->name + LS(CommittedSuicideMsg));
				common.font.drawText(renderer.bmp, msg, rect.x1 + 3, bannerY + 1, 0);
				common.font.drawText(renderer.bmp, msg, rect.x1 + 2, bannerY, 50);
			}
			else
			{
				std::string msg(game.worms[worm.lastKilledByIdx]->settings->name + " killed " + worm.settings->name);
				common.font.drawText(renderer.bmp, msg, rect.x1 + 3, bannerY + 1, 0);
				common.font.drawText(renderer.bmp, msg, rect.x1 + 2, bannerY, 50);
			}
		}
	}

	auto br = game.bonuses.all();
	for (Bonus* i; i = br.next(); )
	{
		if(i->timer > LC(BonusFlickerTime) || (game.cycles & 3) == 0)
		{
			int f = common.bonusFrames[i->frame];

			blitImage(
				renderer.bmp,
				common.smallSprites[f],
				ftoi(i->x) - 3 + offs.x,
				ftoi(i->y) - 3 + offs.y);

			if(game.settings->shadow)
			{
				blitShadowImage(
					common,
					renderer.bmp,
					common.smallSprites.spritePtr(f),
					ftoi(i->x) - 5 + offs.x, // TODO: Use offsX
					ftoi(i->y) - 1 + offs.y,
					7, 7);
			}

			if(game.settings->namesOnBonuses
			&& i->frame == 0)
			{
				std::string const& name = common.weapons[i->weapon].name;
				int len = int(name.size()) * 4;

				common.drawTextSmall(
					renderer.bmp,
					name.c_str(),
					ftoi(i->x) - len/2 + offs.x,
					ftoi(i->y) - 10 + offs.y);
			}
		}
	}

	auto sr = game.sobjects.all();
	for (SObject* i; i = sr.next(); )
	{
		SObjectType const& t = common.sobjectTypes[i->id];
		int frame = i->curFrame + t.startFrame;

		blitImageR(
			renderer.bmp,
			common.largeSprites.spritePtr(frame),
			i->x + offs.x,
			i->y + offs.y,
			16, 16);

		if(game.settings->shadow)
		{
			blitShadowImage(
				common,
				renderer.bmp,
				common.largeSprites.spritePtr(frame),
				i->x + offs.x - 3,
				i->y + offs.y + 3, // TODO: Original doesn't offset the shadow, which is clearly wrong. Check that this offset is correct.
				16, 16);
		}
	}

	auto wr = game.wobjects.all();
	for (WObject* i; i = wr.next(); )
	{
		Weapon const& w = *i->type;

		if(w.startFrame > -1)
		{
			int curFrame = i->curFrame;
			int shotType = w.shotType;

			if(shotType == 2)
			{
				curFrame += 4;
				curFrame >>= 3;
				if(curFrame < 0)
					curFrame = 16;
				else if(curFrame > 15)
					curFrame -= 16;
			}
			else if(shotType == 3)
			{
				if(curFrame > 64)
					--curFrame;
				curFrame -= 12;
				curFrame >>= 3;
				if(curFrame < 0)
					curFrame = 0;
				else if(curFrame > 12)
					curFrame = 12;
			}

			int posX = ftoi(i->pos.x) - 3;
			int posY = ftoi(i->pos.y) - 3;

			if(game.settings->shadow
			&& w.shadow)
			{
				blitShadowImage(
					common,
					renderer.bmp,
					common.smallSprites.spritePtr(w.startFrame + curFrame),
					posX - 3 + offs.x,
					posY + 3 + offs.y,
					7, 7);
			}

			blitImage(
				renderer.bmp,
				common.smallSprites[w.startFrame + curFrame],
				posX + offs.x,
				posY + offs.y);
		}
		else if(i->curFrame > 0)
		{
			int posX = ftoi(i->pos.x) - x + rect.x1;
			int posY = ftoi(i->pos.y) - y + rect.y1;

			if(renderer.bmp.clip_rect.inside(posX, posY))
				renderer.bmp.getPixel(posX, posY) = static_cast<PalIdx>(i->curFrame);

			if(game.settings->shadow)
			{
				posX -= 3;
				posY += 3;

				if(renderer.bmp.clip_rect.inside(posX, posY))
				{
					PalIdx& pix = renderer.bmp.getPixel(posX, posY);
					if(common.materials[pix].seeShadow())
						pix += 4;
				}
			}

		}

		if(!common.H[HRemExp] && i->type - &common.weapons[0] == 34 && game.settings->namesOnBonuses) // TODO: Read from EXE
		{
			if(i->curFrame == 0)
			{
				int nameNum = int(&*i - game.wobjects.arr) % (int)common.weapons.size(); // TODO: Something nicer maybe

				std::string const& name = common.weapons[nameNum].name;
				int width = int(name.size()) * 4;

				common.drawTextSmall(
					renderer.bmp,
					name.c_str(),
					ftoi(i->pos.x) - width/2 + offs.x,
					ftoi(i->pos.y) - 10 + offs.y);
			}
		}
	}

	auto nr = game.nobjects.all();
	for (NObject* i; i = nr.next(); )
	{
		NObjectType const& t = *i->type;

		if(t.startFrame > 0)
		{
			auto pos = ftoi(i->pos) - gvl::ivec2(3, 3);

			if(game.settings->shadow)
			{
				blitShadowImage(
					common,
					renderer.bmp,
					common.smallSprites.spritePtr(t.startFrame + i->curFrame),
					pos.x - 3 + offs.x,
					pos.y + 3 + offs.y,
					7,
					7);
			}

			blitImage(
				renderer.bmp,
				common.smallSprites[t.startFrame + i->curFrame],
				pos.x + offs.x,
				pos.y + offs.y);

		}
		else if(i->curFrame > 1)
		{
			auto pos = ftoi(i->pos) + offs;
			if(renderer.bmp.clip_rect.encloses(pos))
				renderer.bmp.getPixel(pos.x, pos.y) = PalIdx(i->curFrame);

			if(game.settings->shadow)
			{
				pos.x -= 3;
				pos.y += 3;

				if(renderer.bmp.clip_rect.encloses(pos))
				{
					PalIdx& pix = renderer.bmp.getPixel(pos.x, pos.y);
					if(common.materials[pix].seeShadow())
						pix += 4;
				}
			}
		}
	}

	for(std::size_t i = 0; i < game.worms.size(); ++i)
	{
		Worm const& w = *game.worms[i];

		if(w.visible)
		{

			int tempX = ftoi(w.pos.x) - 7 + offs.x;
			int tempY = ftoi(w.pos.y) - 5 + offs.y;
			int angleFrame = w.angleFrame();

			if(w.weapons[w.currentWeapon].available())
			{
				int hotspotX = w.hotspotX + offs.x;
				int hotspotY = w.hotspotY + offs.y;

				WormWeapon const& ww = w.weapons[w.currentWeapon];
				Weapon const& weapon = *ww.type;

				if(weapon.laserSight)
				{
					drawLaserSight(renderer.bmp, rand, hotspotX, hotspotY, tempX + 7, tempY + 4);
				}

				if(ww.type - &common.weapons[0] == LC(LaserWeapon) - 1 && w.pressed(Worm::Fire))
				{
					drawLine(renderer.bmp, hotspotX, hotspotY, tempX + 7, tempY + 4, weapon.colorBullets);
				}
			}

			if(w.ninjarope.out)
			{
				int ninjaropeX = ftoi(w.ninjarope.pos.x) + offs.x;
				int ninjaropeY = ftoi(w.ninjarope.pos.y) + offs.y;

				drawNinjarope(common, renderer.bmp, ninjaropeX, ninjaropeY, tempX + 7, tempY + 4);

				blitImage(renderer.bmp, common.largeSprites[84], ninjaropeX - 1, ninjaropeY - 1);

				if(game.settings->shadow)
				{
					drawShadowLine(common, renderer.bmp, ninjaropeX - 3, ninjaropeY + 3, tempX + 7 - 3, tempY + 4 + 3);
					blitShadowImage(common, renderer.bmp, common.largeSprites.spritePtr(84), ninjaropeX - 4, ninjaropeY + 2, 16, 16);
				}

			}

			if(w.weapons[w.currentWeapon].type->fireCone > 0 && w.fireCone > 0)
			{
				/* TODO
				//NOTE! Check fctab so it's correct
				//NOTE! Check function 1071C and see what it actually does*/

				blitFireCone(
					renderer.bmp,
					w.fireCone / 2,
					common.fireConeSprite(angleFrame, w.direction),
					common.fireConeOffset[w.direction][angleFrame][0] + tempX,
					common.fireConeOffset[w.direction][angleFrame][1] + tempY);
			}


			blitImage(renderer.bmp, common.wormSpriteObj(w.currentFrame, w.direction, w.index), tempX, tempY);
			if(game.settings->shadow)
				blitShadowImage(common, renderer.bmp, common.wormSprite(w.currentFrame, w.direction, w.index), tempX - 3, tempY + 3, 16, 16);
		}

		if (w.ai)
			w.ai->drawDebug(game, w, renderer, offs.x, offs.y);
	}

	/*
	auto& dp = gfx.debugPoints;

	for (auto& p : dp)
	{
		int x = ftoi(p.first) + offsX;
		int y = ftoi(p.second) + offsY;

		if(isInside(renderer.bmp.clip_rect, x, y))
			renderer.bmp.getPixel(x, y) = 0;
	}*/

	for(std::size_t i = 0; i < game.worms.size(); ++i)
	{
		Worm const& worm = *game.worms[i];
		if(worm.visible)
		{
			auto temp = ftoi(worm.pos) - gvl::ivec2(1, 2) + ftoi(cossinTable[ftoi(worm.aimingAngle)] * 16) + offs;
			//int tempX = ftoi(worm.pos.x) - 1 + ftoi(cosTable[ftoi(worm.aimingAngle)] * 16) + offs.x;
			//int tempY = ftoi(worm.pos.y) - 2 + ftoi(sinTable[ftoi(worm.aimingAngle)] * 16) + offs.y;

			blitImage(
				renderer.bmp,
				common.smallSprites[worm.makeSightGreen ? 44 : 43],
				temp.x,
				temp.y);

			if(worm.pressed(Worm::Change))
			{
				std::string const& name = worm.weapons[worm.currentWeapon].type->name;

				int len = int(name.size()) * 4; // TODO: Read 4 from exe? (SW_CHARWID)

				common.drawTextSmall(
					renderer.bmp,
					name.c_str(),
					ftoi(worm.pos.x) - len/2 + 1 + offs.x,
					ftoi(worm.pos.y) - 10 + offs.y);
			}
		}
	}

	for(Game::BObjectList::iterator i = game.bobjects.begin(); i != game.bobjects.end(); ++i)
	{
		auto ipos = ftoi(i->pos) + offs;
		if(renderer.bmp.clip_rect.encloses(ipos))
			renderer.bmp.getPixel(ipos.x, ipos.y) = PalIdx(i->color);

		if(game.settings->shadow)
		{
			ipos.x -= 3;
			ipos.y += 3;

			if(renderer.bmp.clip_rect.encloses(ipos))
			{
				PalIdx& pix = renderer.bmp.getPixel(ipos.x, ipos.y);
				if(common.materials[pix].seeShadow())
					pix += 4;
			}
		}
	}
}
