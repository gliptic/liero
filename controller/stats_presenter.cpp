#include "stats_presenter.hpp"

#include <algorithm>
#include <type_traits>
#include <gvl/io/encoding.hpp>
#include "text.hpp"
#include "stats.hpp"
#include "game.hpp"
#include "gfx.hpp"

using gvl::cell;
using std::vector;

std::string percent(int nom, int den)
{
	if (den == 0)
		return "";
	
	char buf[256];
	sprintf(buf, "%.2f%%", double(nom)*100.0/den);
	return buf;
}

struct StatsRenderer
{
	static int const textColor = 7;

	StatsRenderer(
		Renderer& renderer,
		Game& game,
		NormalStatsRecorder& stats,
		Common& common)
	: renderer(renderer)
	, game(game)
	, stats(stats)
	, common(common)
	{
		
	}

	static int const paneX = 10;
	static int const paneWidth = 320-20;

	template<typename P>
	void pane(int n, int leftX, int topY, P const& p)
	{
		offsX = n * 320 + leftX;

		if (offsX >= -320 && offsX < 320)
		{
			y = topY;
			y += 10;

			drawRoundedBox(renderer.screenBmp, offsX + paneX, y, 0, 2000, paneWidth);

			y += 10;

			p();
		}
	}

	template<typename B>
	bool hblock(int height, B const& b)
	{
		bool ran = false;
		if (y < 200
		 && y + height > 0)
		{
			b();
			ran = true;
		}
		y += height;
		return ran;
	}

	void drawWorms()
	{
		hblock(20, [this] {
			for (int i = 0; i < 2; ++i)
			{
				int x = 160 + (i == 0 ? -1 : 1) * (160 / 2) + offsX;
				blitImage(renderer.screenBmp, common.wormSpriteObj(2, i == 0 ? 1 : 0, i), x - 8, y);

				common.font.drawText(
					renderer.screenBmp,
					cell(i == 0 ? cell::right : cell::left) << game.worms[i]->settings->name,
					x + (i == 0 ? -16 : 16),
					y + 2,
					textColor);
			}
		});
	}

	void drawWorm(int i)
	{
		bool visible = hblock(20, [this, i] {
			int x = 160 + offsX;
			blitImage(renderer.screenBmp, common.wormSpriteObj(2, i == 0 ? 1 : 0, i), x - 8, y);
			common.font.drawText(
				renderer.screenBmp,
				cell(i == 0 ? cell::right : cell::left) << game.worms[i]->settings->name,
				x + (i == 0 ? -16 : 16),
				y + 2,
				textColor);
		});

		if (!visible)
		{
			int x = 18 + offsX;
			blitImage(renderer.screenBmp, common.wormSpriteObj(2, i == 0 ? 1 : 0, i), x - 8, 10);
		}
	}

	template<typename Ws>
	void drawWormStat(char const* name, Ws wormStat)
	{
		hblock(11, [this, name, &wormStat] {
			common.font.drawText(
				renderer.screenBmp,
				cell(cell::center) << name, 160 + offsX, y, textColor);

			for (int i = 0; i < 2; ++i)
			{
				cell::placement p = i == 0 ? cell::right : cell::left;
				int x = 160 + (i == 0 ? -40 : 40) + offsX;

				WormStats& w = stats.worms[i];
				cell c(p);
				wormStat(w, c);
				common.font.drawText(
					renderer.screenBmp,
					c, x, y, textColor);
			}
		});
	}

	template<typename Stat>
	void drawStat(char const* name, Stat stat)
	{
		hblock(11, [this, name, &stat] {
			common.font.drawText(
				renderer.screenBmp,
				cell(cell::right) << name, 160 + offsX, y, textColor);

			int x = 160 + 10 + offsX;

			cell c(cell::left);
			stat(c);
			common.font.drawText(
				renderer.screenBmp,
				c, x, y, textColor);
		});
	}

	void drawWormStat(char const* name, int (WormStats::*field))
	{
		drawWormStat(name, [field](WormStats& w, cell& c) { c << w.*field; });
	}

	void section(cell const& c, int level = 1)
	{
		int x = offsX + 20;
		int color = textColor;
		if (level > 0)
		{
			x += 20;
			color = 3;
		}

		common.font.drawText(
			renderer.screenBmp,
			c, x, y, color);

		if (level == 0)
			y += 11;
	}

	void gap(int n = 5)
	{
		y += n;
	}

	void weaponStats(vector<WeaponStats> const& list)
	{
		for (auto& ws : list)
		{
			if (ws.totalHp > 0)
			{
				section(gvl::cell() << common.weapons[ws.index].name);
				drawStat("hits", [ws](cell& c) {
					c << ws.actualHits << "/" << ws.potentialHits
						<< " (" << percent(ws.actualHits, ws.potentialHits) << ")";
				});
				if (ws.potentialHp > 0)
				{
					drawStat("damage", [ws](cell& c) {
						c << ws.actualHp << "/" << ws.potentialHp
							<< " (" << percent(ws.actualHp, ws.potentialHp) << ")";
					});
				}
				if (ws.totalHp != ws.actualHp)
				{
					drawStat("total damage", [ws](cell& c) {
						c << ws.totalHp;
					});
				}
				gap();
			}
		}
	}

	void graph(vector<double> const& data, int height, int color, int negColor, bool balanced)
	{
		y += 2;
		hblock(height, [&, this] {
			int start = 20 + offsX;
			drawGraph(renderer.screenBmp, data, height, start, y, color, negColor, balanced);
		});
		y += 7;
	}

	void heatmap(Heatmap& hm)
	{
		y += 2;
		hblock(hm.height, [&] {
			int startX = paneX + paneWidth / 2 - (hm.width / 2) + offsX;
			int startY = y;

			drawHeatmap(renderer.screenBmp, startX, startY, hm);
		});
		y += 7;
	}

	Renderer& renderer;
	Game& game;
	NormalStatsRecorder& stats;
	Common& common;
	int offsX, y;
};

void sortWeaponStats(vector<WeaponStats>& ws)
{
	std::sort(ws.begin(), ws.end(),
		[](WeaponStats const& a, WeaponStats const& b) { return a.actualHp > b.actualHp; });
}

void presentStats(NormalStatsRecorder& recorder, Game& game)
{
	gfx.clearKeys();
	
	Common& common = *game.common;

	Bitmap bg;

	bg.copy(gfx.screenBmp);

	StatsRenderer renderer(gfx, game, recorder, common);

	double offset = 0, destOffset = 0;
	double pane = 0;
	double destPane = pane;

	vector<WeaponStats> combinedWeaponStats, weaponStats[2];

	vector<double> wormDamages[2], wormTotalHp[2];

	int const graphWidth = 280;

	for (int i = 0; i < 2; ++i)
	{
		wormDamages[i] = stretch(
			convert<double>(pluck(recorder.worms[i].wormFrameStats, &WormFrameStats::damage)), graphWidth);
		cumulative(wormDamages[i]);
		normalize(wormDamages[i], 50);

		wormTotalHp[i] = convert<double>(pluck(recorder.worms[i].wormFrameStats, &WormFrameStats::totalHp));
	}

	vector<double> wormTotalHpDiff(
		stretch(zip(wormTotalHp[0], wormTotalHp[1], std::minus<double>()), graphWidth));

	normalize(wormTotalHpDiff, 100);
	
	for (int i = 0; i < 40; ++i)
	{
		auto ws = recorder.worms[0].weapons[i];
		ws.combine(recorder.worms[1].weapons[i]);
		combinedWeaponStats.push_back(ws);

		weaponStats[0].push_back(recorder.worms[0].weapons[i]);
		weaponStats[1].push_back(recorder.worms[1].weapons[i]);
	}

	sortWeaponStats(combinedWeaponStats);
	sortWeaponStats(weaponStats[0]);
	sortWeaponStats(weaponStats[1]);

	while (true)
	{
		gfx.screenBmp.copy(bg);

		int offsX = (int)std::floor(pane * -320);
		int offsY = (int)offset;

		renderer.pane(0, offsX, offsY, [&] {
			renderer.drawWorms();

			int oldy = renderer.y;
			renderer.y -= 20;

			renderer.drawStat(
				common.texts.gameModes[game.settings->gameMode].c_str(),
				[&](cell& c) { c << timeToStringFrames(recorder.gameTime); });

			renderer.y = oldy;

			if (game.settings->gameMode == Settings::GMHoldazone
			 || game.settings->gameMode == Settings::GMGameOfTag)
			{
				renderer.drawWormStat("timer", [&](WormStats& w, cell& c) {
					c << timeToString(w.timer);
				});
			}
			else
			{
				renderer.drawWormStat("lives left", &WormStats::lives);
			}
			renderer.drawWormStat("kills", &WormStats::kills);
			renderer.drawWormStat("damage dealt", &WormStats::damageDealt);
			renderer.drawWormStat("damage received", &WormStats::damage);
			renderer.drawWormStat("damage to self", &WormStats::selfDamage);
		
			renderer.drawWormStat("shortest life", [](WormStats& w, cell& c) {
				int min, max;
				w.lifeStats(min, max);
				c << timeToStringFrames(min);
			});

			renderer.drawWormStat("longest life", [](WormStats& w, cell& c) {
				int min, max;
				w.lifeStats(min, max);
				c << timeToStringFrames(max);
			});

			renderer.gap();

			renderer.weaponStats(combinedWeaponStats);

			renderer.section(cell() << "Total health difference", 0);
			
			renderer.graph(
					wormTotalHpDiff,
					100,
					Palette::wormColourIndexes[0],
					Palette::wormColourIndexes[1],
					true);

			renderer.section(cell() << "Presence", 0);
			renderer.heatmap(recorder.presence);
		});

		for (int i = 0; i < 2; ++i)
		{
			WormStats& wormStats = recorder.worms[i];
			renderer.pane(i == 0 ? -1 : 1, offsX, offsY, [&] {
				renderer.drawWorm(i);

				renderer.weaponStats(weaponStats[i]);

				renderer.section(cell() << "Damage over time", 0);
				renderer.graph(wormDamages[i], 50, Palette::wormColourIndexes[i], 0, false);

				renderer.section(cell() << "Presence", 0);
				renderer.heatmap(wormStats.presence);
				renderer.section(cell() << "Damage", 0);
				renderer.heatmap(wormStats.damageHm);
			});
		}

		gfx.pal = common.exepal; // We don't use gfx.origpal because the colors are unpredictable

		gfx.flip();
		gfx.process();

		if (gfx.testSDLKey(SDLK_DOWN))
		{
			destOffset = destOffset - 10;
		}
		else if (gfx.testSDLKey(SDLK_UP))
		{
			destOffset = std::min(destOffset + 10.0, 0.0);
		}
		else if (gfx.testSDLKeyOnce(SDLK_RIGHT))
		{
			destPane = std::min(destPane + 1.0, 1.0);
		}
		else if (gfx.testSDLKeyOnce(SDLK_LEFT))
		{
			destPane = std::max(destPane - 1.0, -1.0);
		}
		else if (gfx.testSDLKey(SDLK_RETURN) || gfx.testSDLKey(SDLK_ESCAPE))
		{
			break;
		}

		pane = (pane * 0.89 + destPane * 0.11);
		offset = (offset * 0.89 + destOffset * 0.11);

		/*
		vel = vel * 89 / 100;

		if (vel < -itof(3))
			vel = -itof(3);
		else if (vel > itof(3))
			vel = itof(3);

		offset += vel;*/

		if (offset > 0)
		{
			offset = 0;
			//vel = 0;
		}
	}
	
	fill(gfx.screenBmp, 0);

	gfx.clearKeys();
}