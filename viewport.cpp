#include "viewport.hpp"
//#include "gfx.hpp"
#include "game.hpp"
#include "text.hpp"
#include "math.hpp"
#include "constants.hpp"
#include "gfx/bitmap.hpp"
#include "gfx/renderer.hpp"
#include "gfx/blit.hpp"

//#include <iostream>

struct PreserveClipRect
{
	PreserveClipRect(Bitmap& bmp)
	: bmp(bmp)
	{
		rect = bmp.clip_rect;
	}
	
	~PreserveClipRect()
	{
		bmp.clip_rect = rect;
	}
	
	Bitmap& bmp;
	Rect rect;
};

void Viewport::process(Game& game)
{
	Worm& worm = *game.wormByIdx(wormIdx);
	if(worm.killedTimer <= 0)
	{
		if(worm.visible)
		{
			if(worm.steerableCount > 0)
			{
				setCenter(worm.steerableSumX / worm.steerableCount, worm.steerableSumY / worm.steerableCount);
			}
			else
			{
				setCenter(ftoi(worm.x), ftoi(worm.y));
			}
		}
		else
		{
			scrollTo(ftoi(worm.x), ftoi(worm.y), 4);
		}
	}
	else if(worm.health < 0)
	{
		setCenter(ftoi(worm.x), ftoi(worm.y));
		
		if(worm.killedTimer == 150) // TODO: This depends on what is the starting killedTimer
			bannerY = -8;
	}
	
	int realShake = ftoi(shake);
	
	if(realShake > 0)
	{
		x += rand(realShake * 2) - realShake;
		y += rand(realShake * 2) - realShake;
	}
	
	if(x < 0) x = 0;
	if(y < 0) y = 0;
	if(x > maxX) x = maxX;
	if(y > maxY) y = maxY;
	
	/*
	if(worm->health <= 0)
	{
		if((game.cycles & 1) == 0)
		{
			if(worm->killedTimer > 16)
			{
				if(bannerY < 2)
					++bannerY;
			}
			else
			{
				if(bannerY > -8)
					--bannerY;
			}
		}
	}*/
}

void Viewport::draw(Game& game, Renderer& renderer, bool isReplay)
{
	Common& common = *game.common;
	Worm& worm = *game.wormByIdx(wormIdx);

	if(worm.visible)
	{
		int lifebarWidth = worm.health * 100 / worm.settings->health;
		drawBar(renderer.screenBmp, inGameX, 161, lifebarWidth, lifebarWidth/10 + 234);
	}
	else
	{
		int lifebarWidth = 100 - (worm.killedTimer * 25) / 37;
		if(lifebarWidth > 0)
		{
			if(lifebarWidth > 100)
				lifebarWidth = 100;
			drawBar(renderer.screenBmp, inGameX, 161, lifebarWidth, lifebarWidth/10 + 234);
		}
	}
	
	// Draw kills status
	
	WormWeapon const& ww = worm.weapons[worm.currentWeapon];
	
	if(ww.available)
	{
		if(ww.ammo > 0)
		{
			int ammoBarWidth = ww.ammo * 100 / common.weapons[ww.id].ammo;
			
			if(ammoBarWidth > 0)
				drawBar(renderer.screenBmp, inGameX, 166, ammoBarWidth, ammoBarWidth/10 + 245);
		}
	}
	else
	{
		int ammoBarWidth = 0;
		
		if(common.weapons[ww.id].loadingTime != 0)
		{
			int computedLoadingTime = common.weapons[ww.id].computedLoadingTime(*game.settings);
			ammoBarWidth = 100 - ww.loadingLeft * 100 / computedLoadingTime;
		}
		else
		{
			ammoBarWidth = 100 - ww.loadingLeft * 100;
		}
		
		if(ammoBarWidth > 0)
			drawBar(renderer.screenBmp, inGameX, 166, ammoBarWidth, ammoBarWidth/10 + 245);
		
		if((game.cycles % 20) > 10
		&& worm.visible)
		{
			common.font.drawText(renderer.screenBmp, LS(Reloading), inGameX, 164, 50);
		}
	}
	
	common.font.drawText(renderer.screenBmp, (LS(Kills) + toString(worm.kills)), inGameX, 171, 10);
	
	if(isReplay)
	{
		common.font.drawText(renderer.screenBmp, worm.settings->name, inGameX, 192, 4);
		common.font.drawText(renderer.screenBmp, timeToStringEx(game.cycles * 14), 95, 185, 7);
	}

	int const stateColours[2][2] = {{6, 10}, {79, 4}};
	
	switch(game.settings->gameMode)
	{
	case Settings::GMKillEmAll:
	{
		common.font.drawText(renderer.screenBmp, (LS(Lives) + toString(worm.lives)), inGameX, 178, 6);
	}
	break;

	case Settings::GMHoldazone:
	{
		int state = 0;
		
		for (auto* w : game.worms)
			if (w != &worm && w->timer <= worm.timer)
				state = 1;
		
		int color = stateColours[game.holdazone.holderIdx != worm.index][state];
		
		common.font.drawText(renderer.screenBmp, timeToString(worm.timer), 5, 106 + 84*worm.index, 161, color);
	}
	break;
	
	case Settings::GMGameOfTag:
	{
		int state = 0;
		
		for (auto* w : game.worms)
			if (w != &worm && w->timer >= worm.timer)
				state = 1;

		int color = stateColours[game.lastKilledIdx != worm.index][state];
		
		common.font.drawText(renderer.screenBmp, timeToString(worm.timer), 5, 106 + 84*worm.index, 161, color);
	}
	break;
	}	

	{
		PreserveClipRect pcr(renderer.screenBmp);
	
		renderer.screenBmp.clip_rect = rect;
	
		int offsX = rect.x1 - x;
		int offsY = rect.y1 - y;
	
		blitImageNoKeyColour(renderer.screenBmp, &game.level.data[0], offsX, offsY, game.level.width, game.level.height);

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

			drawDashedLineBox(renderer.screenBmp,
				game.holdazone.rect.x1 + offsX,
				game.holdazone.rect.y1 + offsY,
				color,
				contenderColor,
				game.holdazone.contenderFrames,
				game.settings->zoneCaptureTime,
				game.holdazone.rect.width(),
				game.holdazone.rect.height(), game.cycles / 10);
		}
	
		if(!worm.visible
		&& worm.killedTimer <= 0
		&& !worm.ready)
		{
			common.font.drawText(renderer.screenBmp, LS(PressFire), rect.center_x() - 30, 76, 0);
			common.font.drawText(renderer.screenBmp, LS(PressFire), rect.center_x() - 31, 75, 50);
		}

		if(bannerY > -8
		&& worm.health <= 0)
		{	
			if(game.settings->gameMode == Settings::GMGameOfTag
			&& game.gotChanged)
			{
				common.font.drawText(renderer.screenBmp, LS(YoureIt), rect.x1 + 3, bannerY + 1, 0);
				common.font.drawText(renderer.screenBmp, LS(YoureIt), rect.x1 + 2, bannerY, 50);
			}
		}
	
		for(std::size_t i = 0; i < game.viewports.size(); ++i)
		{
			Viewport* v = game.viewports[i];
			Worm& otherWorm = *game.wormByIdx(v->wormIdx);
			if(v != this
			&& otherWorm.health <= 0
			&& v->bannerY > -8)
			{
				if(otherWorm.lastKilledByIdx == worm.index)
				{
					std::string msg(LS(KilledMsg) + otherWorm.settings->name);
					common.font.drawText(renderer.screenBmp, msg, rect.x1 + 3, v->bannerY + 1, 0);
					common.font.drawText(renderer.screenBmp, msg, rect.x1 + 2, v->bannerY, 50);
				}
				else
				{
					std::string msg(otherWorm.settings->name + LS(CommittedSuicideMsg));
					common.font.drawText(renderer.screenBmp, msg, rect.x1 + 3, v->bannerY + 1, 0);
					common.font.drawText(renderer.screenBmp, msg, rect.x1 + 2, v->bannerY, 50);
				}
			}
		}

		for(Game::BonusList::iterator i = game.bonuses.begin(); i != game.bonuses.end(); ++i)
		{
			if(i->timer > LC(BonusFlickerTime) || (game.cycles & 3) == 0)
			{
				int f = common.bonusFrames[i->frame];
			
				blitImage(
					renderer.screenBmp,
					common.smallSprites[f],
					ftoi(i->x) - x - 3 + rect.x1, // TODO: Use offsX
					ftoi(i->y) - y - 3 + rect.y1);
				
				if(game.settings->shadow)
				{
					blitShadowImage(
						common,
						renderer.screenBmp,
						common.smallSprites.spritePtr(f),
						ftoi(i->x) - 5 + offsX, // TODO: Use offsX
						ftoi(i->y) - 1 + offsY,
						7, 7);
				}
			
				if(game.settings->namesOnBonuses
				&& i->frame == 0)
				{
					std::string const& name = common.weapons[i->weapon].name;
					int len = int(name.size()) * 4;
				
					common.drawTextSmall(
						renderer.screenBmp,
						name.c_str(),
						ftoi(i->x) - len/2 + offsX,
						ftoi(i->y) - 10 + offsY);
				}
			}
		}
		
		for(Game::SObjectList::iterator i = game.sobjects.begin(); i != game.sobjects.end(); ++i)
		{
			SObjectType const& t = common.sobjectTypes[i->id];
			int frame = i->curFrame + t.startFrame;
		
			// TODO: Check that blitImageR is the correct one to use (probably)
			blitImageR(
				renderer.screenBmp,
				common.largeSprites.spritePtr(frame),
				i->x + offsX,
				i->y + offsY,
				16, 16);
			
			if(game.settings->shadow)
			{
				blitShadowImage(
					common,
					renderer.screenBmp,
					common.largeSprites.spritePtr(frame),
					i->x + offsX - 3,
					i->y + offsY + 3, // TODO: Original doesn't offset the shadow, which is clearly wrong. Check that this offset is correct.
					16, 16);
			}
		}

		for(Game::WObjectList::iterator i = game.wobjects.begin(); i != game.wobjects.end(); ++i)
		{
			Weapon const& w = common.weapons[i->id];
		
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
			
				int posX = ftoi(i->x) - 3;
				int posY = ftoi(i->y) - 3;
			
				if(game.settings->shadow
				&& w.shadow)
				{
					blitShadowImage(
						common,
						renderer.screenBmp,
						common.smallSprites.spritePtr(w.startFrame + curFrame),
						posX - 3 + offsX,
						posY + 3 + offsY,
						7, 7);
				}
			
				blitImage(
					renderer.screenBmp,
					common.smallSprites[w.startFrame + curFrame],
					posX + offsX,
					posY + offsY);
			}
			else if(i->curFrame > 0)
			{
				int posX = ftoi(i->x) - x + rect.x1;
				int posY = ftoi(i->y) - y + rect.y1;
			
				if(renderer.screenBmp.clip_rect.inside(posX, posY))
					renderer.screenBmp.getPixel(posX, posY) = static_cast<PalIdx>(i->curFrame);
			
				if(game.settings->shadow)
				{
					posX -= 3;
					posY += 3;
				
					if(renderer.screenBmp.clip_rect.inside(posX, posY))
					{
						PalIdx& pix = renderer.screenBmp.getPixel(posX, posY);
						if(common.materials[pix].seeShadow())
							pix += 4;
					}
				}
			
			}
		
			if(!common.H[HRemExp] && i->id == 34 && game.settings->namesOnBonuses) // TODO: Read from EXE
			{
				if(i->curFrame == 0)
				{
					int nameNum = int(&*i - game.wobjects.arr) % 40; // TODO: Something nicer maybe
				
					std::string const& name = common.weapons[nameNum].name;
					int width = int(name.size()) * 4;
				
					common.drawTextSmall(
						renderer.screenBmp,
						name.c_str(),
						ftoi(i->x) - x - width/2 + rect.x1,
						ftoi(i->y) - y - 10 + rect.y1);
				}
			}
		}
	
		for(Game::NObjectList::iterator i = game.nobjects.begin(); i != game.nobjects.end(); ++i)
		{
			NObjectType const& t = common.nobjectTypes[i->id];
		
			if(t.startFrame > 0)
			{
				int posX = ftoi(i->x) - 3;
				int posY = ftoi(i->y) - 3;
			
				if(i->id >= 20 && i->id <= 21)
				{
					// Flag special case
					posY -= 2;
					posX += 3;
				}
			
				if(game.settings->shadow)
				{
					blitShadowImage(
						common,
						renderer.screenBmp,
						common.smallSprites.spritePtr(t.startFrame + i->curFrame),
						posX - 3 + offsX,
						posY + 3 + offsY,
						7,
						7);
				}
			
				blitImage(
					renderer.screenBmp,
					common.smallSprites[t.startFrame + i->curFrame],
					posX + offsX,
					posY + offsY);
				
			}
			else if(i->curFrame > 1)
			{
				int posX = ftoi(i->x) + offsX;
				int posY = ftoi(i->y) + offsY;
				if(renderer.screenBmp.clip_rect.inside(posX, posY))
					renderer.screenBmp.getPixel(posX, posY) = PalIdx(i->curFrame);
				
				if(game.settings->shadow)
				{
					posX -= 3;
					posY += 3;
				
					if(renderer.screenBmp.clip_rect.inside(posX, posY))
					{
						PalIdx& pix = renderer.screenBmp.getPixel(posX, posY);
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
			
				int tempX = ftoi(w.x) - 7 + offsX;
				int tempY = ftoi(w.y) - 5 + offsY;
				int angleFrame = w.angleFrame();
			
				if(w.weapons[w.currentWeapon].available)
				{
					int hotspotX = w.hotspotX + offsX;
					int hotspotY = w.hotspotY + offsY;
				
					WormWeapon const& ww = w.weapons[w.currentWeapon];
					Weapon& weapon = common.weapons[ww.id];
				
					if(weapon.laserSight)
					{
						drawLaserSight(renderer.screenBmp, renderer.rand, hotspotX, hotspotY, tempX + 7, tempY + 4);
					}
				
					if(ww.id == LC(LaserWeapon) - 1 && w.pressed(Worm::Fire))
					{
						drawLine(renderer.screenBmp, hotspotX, hotspotY, tempX + 7, tempY + 4, weapon.colorBullets);
					}
				}
			
				if(w.ninjarope.out)
				{
					int ninjaropeX = ftoi(w.ninjarope.x) - x + rect.x1;
					int ninjaropeY = ftoi(w.ninjarope.y) - y + rect.y1;
				
					drawNinjarope(common, renderer.screenBmp, ninjaropeX, ninjaropeY, tempX + 7, tempY + 4);
				
					blitImage(renderer.screenBmp, common.largeSprites[84], ninjaropeX - 1, ninjaropeY - 1);
				
					if(game.settings->shadow)
					{
						drawShadowLine(common, renderer.screenBmp, ninjaropeX - 3, ninjaropeY + 3, tempX + 7 - 3, tempY + 4 + 3);
						blitShadowImage(common, renderer.screenBmp, common.largeSprites.spritePtr(84), ninjaropeX - 4, ninjaropeY + 2, 16, 16);
					}
				
				}
			
				if(common.weapons[w.weapons[w.currentWeapon].id].fireCone > -1
				&& w.fireConeActive)
				{
					/* TODO
					//NOTE! Check fctab so it's correct
					//NOTE! Check function 1071C and see what it actually does*/
				
					blitFireCone(
						renderer.screenBmp,
						w.fireCone / 2,
						common.fireConeSprite(angleFrame, w.direction),
						common.fireConeOffset[w.direction][angleFrame][0] + tempX,
						common.fireConeOffset[w.direction][angleFrame][1] + tempY);
				}
			
			
				blitImage(renderer.screenBmp, common.wormSpriteObj(w.currentFrame, w.direction, w.index), tempX, tempY);
				if(game.settings->shadow)
					blitShadowImage(common, renderer.screenBmp, common.wormSprite(w.currentFrame, w.direction, w.index), tempX - 3, tempY + 3, 16, 16);
			}

			if (w.ai)
				w.ai->drawDebug(game, w, renderer, offsX, offsY);
		}

		/*
		auto& dp = gfx.debugPoints;

		for (auto& p : dp)
		{
			int x = ftoi(p.first) + offsX;
			int y = ftoi(p.second) + offsY;

			if(isInside(renderer.screenBmp.clip_rect, x, y))
				renderer.screenBmp.getPixel(x, y) = 0;
		}*/
	
		if(worm.visible)
		{
			int tempX = ftoi(worm.x) - x - 1 + ftoi(cosTable[ftoi(worm.aimingAngle)] * 16) + rect.x1;
			int tempY = ftoi(worm.y) - y - 2 + ftoi(sinTable[ftoi(worm.aimingAngle)] * 16) + rect.y1;
		
			if(worm.makeSightGreen)
			{
				blitImage(
					renderer.screenBmp,
					common.smallSprites[44],
					tempX,
					tempY);
			}
			else
			{
				blitImage(
					renderer.screenBmp,
					common.smallSprites[43],
					tempX,
					tempY);
			}

			if(worm.pressed(Worm::Change))
			{
				int id = worm.weapons[worm.currentWeapon].id;
				std::string const& name = common.weapons[id].name;
			
				int len = int(name.size()) * 4; // TODO: Read 4 from exe? (SW_CHARWID)
			
				common.drawTextSmall(
					renderer.screenBmp,
					name.c_str(),
					ftoi(worm.x) - x - len/2 + 1 + rect.x1,
					ftoi(worm.y) - y - 10 + rect.y1);
			}
		}
	
		for(Game::BObjectList::iterator i = game.bobjects.begin(); i != game.bobjects.end(); ++i)
		{
			int posX = ftoi(i->x) + offsX;
			int posY = ftoi(i->y) + offsY;
			if(renderer.screenBmp.clip_rect.inside(posX, posY))
				renderer.screenBmp.getPixel(posX, posY) = PalIdx(i->color);
			
			if(game.settings->shadow)
			{
				posX -= 3;
				posY += 3;
			
				if(renderer.screenBmp.clip_rect.inside(posX, posY))
				{
					PalIdx& pix = renderer.screenBmp.getPixel(posX, posY);
					if(common.materials[pix].seeShadow())
						pix += 4;
				}
			}
		}
	}
	
	if(game.settings->map)
	{
		int const mapX = 134, mapY = 162;
		int my = 5;
		for(int y = mapY; y < 197; ++y)
		{
			int mx = 5;
			for(int x = mapX; x < 185; ++x)
			{
				renderer.screenBmp.getPixel(x, y) = game.level.checkedPixelWrap(mx, my);
				mx += 10;
			}
			my += 10;
		}
		
		for(std::size_t i = 0; i < game.worms.size(); ++i)
		{
			Worm const& w = *game.worms[i];
			
			if(w.visible)
			{
				int x = ftoi(w.x) / 10 + mapX;
				int y = ftoi(w.y) / 10 + mapY;
				
				renderer.screenBmp.getPixel(x, y) = w.minimapColor();
			}
		}

		if (game.settings->gameMode == Settings::GMHoldazone
		 && game.holdazone.timeoutLeft > 0)
		{
			int x = game.holdazone.rect.center_x() / 10 + mapX;
			int y = game.holdazone.rect.center_y() / 10 + mapY;

			int color = 168;

			if (game.holdazone.holderIdx >= 0)
			{
				Worm* holder = game.wormByIdx(game.holdazone.holderIdx);
				color = holder->minimapColor();
			}

			renderer.screenBmp.setPixel(x-1, y, color);
			renderer.screenBmp.setPixel(x+1, y, color);
			renderer.screenBmp.setPixel(x, y-1, color);
			renderer.screenBmp.setPixel(x, y+1, color);
			renderer.screenBmp.setPixel(x-1, y-1, color);
			renderer.screenBmp.setPixel(x+1, y-1, color);
			renderer.screenBmp.setPixel(x-1, y+1, color);
			renderer.screenBmp.setPixel(x+1, y+1, color);
		}
	}
}
