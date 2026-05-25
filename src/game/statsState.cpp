#include "statsState.hpp"

#include <algorithm>
#include <chrono>
#include <type_traits>
#include "gfx/text_cell.hpp"
#include "text.hpp"
#include "stats.hpp"
#include "game.hpp"
#include "gfx.hpp"
#include "rematchState.hpp"
#include "net/session.hpp"

using cell = TextCell;
using std::vector;

static std::string percent(int nom, int den)
{
	if (den == 0)
		return "";

	const int BUF_MAX = 256;
	char buf[BUF_MAX];
	std::snprintf(buf, BUF_MAX * sizeof(char), "%.2f%%", double(nom)*100.0/den);
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
	, paneWidth(renderer.renderResX - 20)
	{
	}

	static int const paneX = 10;

	template<typename P>
	void pane(int n, int leftX, int topY, P const& p)
	{
		offsX = n * renderer.renderResX + leftX;

		if (offsX >= -renderer.renderResX && offsX < renderer.renderResX)
		{
			y = topY;
			y += 10;

			drawRoundedBox(renderer.bmp, offsX + paneX, y, 0, 2000, paneWidth);

			y += 10;

			p();
		}
	}

	template<typename B>
	bool hblock(int height, B const& b)
	{
		bool ran = false;
		if (y < renderer.renderResY
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
				int x = renderer.renderResX / 2 + (i == 0 ? -1 : 1) * (renderer.renderResX / 4) + offsX;
				blitImage(renderer.bmp, common.wormSpriteObj(2, i == 0 ? 1 : 0, i), x - 8, y);

				cell c(i == 0 ? TextCell::Right : TextCell::Left);
				common.font.drawText(
					renderer.bmp,
					c << game.worms[i]->settings->name,
					x + (i == 0 ? -16 : 16),
					y + 2,
					textColor);
			}
		});
	}

	void drawWorm(int i)
	{
		bool visible = hblock(20, [this, i] {
			int x = renderer.renderResX / 2 + offsX;
			blitImage(renderer.bmp, common.wormSpriteObj(2, i == 0 ? 1 : 0, i), x - 8, y);

			cell c(i == 0 ? TextCell::Right : TextCell::Left);
			common.font.drawText(
				renderer.bmp,
				c << game.worms[i]->settings->name,
				x + (i == 0 ? -16 : 16),
				y + 2,
				textColor);
		});

		if (!visible)
		{
			int x = 18 + offsX;
			blitImage(renderer.bmp, common.wormSpriteObj(2, i == 0 ? 1 : 0, i), x - 8, 10);
		}
	}

	template<typename Ws>
	void drawWormStat(char const* name, Ws wormStat)
	{
		hblock(11, [this, name, &wormStat] {
			common.font.drawText(
				renderer.bmp,
				cell(TextCell::Center).ref() << name, renderer.renderResX / 2 + offsX, y, textColor);

			for (int i = 0; i < 2; ++i)
			{
				TextCell::Placement p = i == 0 ? TextCell::Right : TextCell::Left;
				int x = renderer.renderResX / 2 + (i == 0 ? -40 : 40) + offsX;

				WormStats& w = stats.worms[i];
				cell c(p);
				wormStat(w, c);
				common.font.drawText(
					renderer.bmp,
					c, x, y, textColor);
			}
		});
	}

	template<typename Stat>
	void drawStat(char const* name, Stat stat)
	{
		hblock(11, [this, name, &stat] {
			common.font.drawText(
				renderer.bmp,
				cell(TextCell::Right).ref() << name, renderer.renderResX / 2 + offsX, y, textColor);

			int x = renderer.renderResX / 2 + 10 + offsX;

			cell c(TextCell::Left);
			stat(c);
			common.font.drawText(
				renderer.bmp,
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
			renderer.bmp,
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
				section(cell().ref() << common.weapons[ws.index].name);
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
			drawGraph(renderer.bmp, data, height, start, y, color, negColor, balanced);
		});
		y += 7;
	}

	void heatmap(Heatmap& hm)
	{
		y += 2;
		hblock(hm.height, [&] {
			int startX = paneX + paneWidth / 2 - (hm.width / 2) + offsX;
			int startY = y;

			drawHeatmap(renderer.bmp, startX, startY, hm);
		});
		y += 7;
	}

	Renderer& renderer;
	Game& game;
	NormalStatsRecorder& stats;
	Common& common;
	int paneWidth = 300;
	int offsX, y;
};

static void sortWeaponStats(vector<WeaponStats>& ws)
{
	std::sort(ws.begin(), ws.end(),
		[](WeaponStats const& a, WeaponStats const& b) { return a.actualHp > b.actualHp; });
}

StatsState::StatsState(NormalStatsRecorder& recorder, Game& game, bool isMultiplayer)
: recorder_(recorder)
, game_(game)
, isMultiplayer_(isMultiplayer)
{
}

void StatsState::enter()
{
	gfx->clearKeys();

	Common& common = *game_.common;

	int graphWidth = gfx->playRenderer.renderResX - 20 - 20;

	for (int i = 0; i < 2; ++i)
	{
		wormDamages_[i] = stretch(
			convert<double>(pluck(recorder_.worms[i].wormFrameStats, &WormFrameStats::damage)), graphWidth);
		cumulative(wormDamages_[i]);
		normalize(wormDamages_[i], 50);
	}

	vector<double> wormTotalHp[2];
	for (int i = 0; i < 2; ++i)
		wormTotalHp[i] = convert<double>(pluck(recorder_.worms[i].wormFrameStats, &WormFrameStats::totalHp));

	wormTotalHpDiff_ = stretch(zip(wormTotalHp[0], wormTotalHp[1], std::minus<double>()), graphWidth);
	normalize(wormTotalHpDiff_, 100);

	for (int i = 0; i < 40; ++i)
	{
		auto ws = recorder_.worms[0].weapons[i];
		ws.combine(recorder_.worms[1].weapons[i]);
		combinedWeaponStats_.push_back(ws);

		weaponStats_[0].push_back(recorder_.worms[0].weapons[i]);
		weaponStats_[1].push_back(recorder_.worms[1].weapons[i]);
	}

	sortWeaponStats(combinedWeaponStats_);
	sortWeaponStats(weaponStats_[0]);
	sortWeaponStats(weaponStats_[1]);

	bg_.copy(gfx->playRenderer.bmp);
}

void StatsState::handleEvent(SDL_Event& ev)
{
	gfx->processEvent(ev);
}

bool StatsState::update()
{
	// Keep the network session alive while viewing stats
	if (isMultiplayer_ && gfx->netSession)
	{
		gfx->netSession->update();
		auto state = gfx->netSession->sessionState();
		if (state == NetSession::Disconnected || state == NetSession::Failed)
		{
			gfx->netSession.reset();
			isMultiplayer_ = false;
		}
	}

	if (gfx->testSDLKey(SDL_SCANCODE_DOWN)
	|| gfx->testControl(WormSettingsExtensions::Down)
	|| gfx->testGamepadDir(SDL_GAMEPAD_BUTTON_DPAD_DOWN))
	{
		destOffset_ -= 10;
	}
	else if (gfx->testSDLKey(SDL_SCANCODE_UP)
	|| gfx->testControl(WormSettingsExtensions::Up)
	|| gfx->testGamepadDir(SDL_GAMEPAD_BUTTON_DPAD_UP))
	{
		destOffset_ = std::min(destOffset_ + 10.0, 0.0);
	}
	else if (gfx->testSDLKeyOnce(SDL_SCANCODE_RIGHT)
	|| gfx->testControlOnce(WormSettingsExtensions::Right)
	|| gfx->testGamepadDirOnce(SDL_GAMEPAD_BUTTON_DPAD_RIGHT))
	{
		destPane_ = std::min(destPane_ + 1.0, 1.0);
	}
	else if (gfx->testSDLKeyOnce(SDL_SCANCODE_LEFT)
	|| gfx->testControlOnce(WormSettingsExtensions::Left)
	|| gfx->testGamepadDirOnce(SDL_GAMEPAD_BUTTON_DPAD_LEFT))
	{
		destPane_ = std::max(destPane_ - 1.0, -1.0);
	}
	else if (gfx->testSDLKeyOnce(SDL_SCANCODE_RETURN) ||
	         gfx->testSDLKeyOnce(SDL_SCANCODE_ESCAPE) ||
	         gfx->testControlOnce(WormSettingsExtensions::Fire) ||
	         gfx->testControlOnce(WormSettingsExtensions::Jump) ||
	         gfx->testGamepadButtonOnce(SDL_GAMEPAD_BUTTON_SOUTH) ||
	         gfx->testGamepadButtonOnce(SDL_GAMEPAD_BUTTON_EAST))
	{
		fill(gfx->playRenderer.bmp, 0);
		gfx->clearKeys();

		if (isMultiplayer_ && gfx->netSession)
		{
			gfx->stateStack.scheduleReplaceTop(
				std::make_unique<RematchState>(game_));
			return true;
		}

		return false; // pop
	}

	pane_ = pane_ * 0.89 + destPane_ * 0.11;
	offset_ = offset_ * 0.89 + destOffset_ * 0.11;

	if (offset_ > 0)
		offset_ = 0;

	return true;
}

void StatsState::draw()
{
	Common& common = *game_.common;

	gfx->playRenderer.bmp.copy(bg_);

	StatsRenderer renderer(gfx->playRenderer, game_, recorder_, common);

	int offsX = (int)std::floor(pane_ * -renderer.renderer.renderResX);
	int offsY = (int)offset_;

	renderer.pane(0, offsX, offsY, [&] {
		renderer.drawWorms();

		int oldy = renderer.y;
		renderer.y -= 20;

		renderer.drawStat(
			common.texts.gameModes[game_.settings->gameMode].c_str(),
			[&](cell& c) { c << timeToStringFrames(recorder_.gameTime); });

		renderer.y = oldy;

		renderer.drawWormStat("ai processing", [&](WormStats& w, cell& c) {
			c << (int)(std::chrono::duration_cast<std::chrono::milliseconds>(w.aiProcessTime).count()) << "ms";
		});

		if (game_.settings->gameMode == Settings::GMHoldazone
		 || game_.settings->gameMode == Settings::GMGameOfTag)
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

		renderer.drawWormStat("loading efficiency", [](WormStats& w, cell& c) {
			c << percent(w.weaponChangeGood, w.weaponChangeGood + w.weaponChangeBad);
		});

		renderer.gap();

		renderer.weaponStats(combinedWeaponStats_);

		renderer.section(cell().ref() << "Total health difference", 0);

		renderer.graph(
				wormTotalHpDiff_,
				100,
				Palette::wormColourIndexes[0],
				Palette::wormColourIndexes[1],
				true);

		renderer.section(cell().ref() << "Presence", 0);
		renderer.heatmap(recorder_.presence);
	});

	for (int i = 0; i < 2; ++i)
	{
		renderer.pane(i == 0 ? -1 : 1, offsX, offsY, [&] {
			renderer.drawWorm(i);

			renderer.weaponStats(weaponStats_[i]);

			renderer.section(cell().ref() << "Damage over time", 0);
			renderer.graph(wormDamages_[i], 50, Palette::wormColourIndexes[i], 0, false);

			renderer.section(cell().ref() << "Presence", 0);
			renderer.heatmap(recorder_.worms[i].presence);
			renderer.section(cell().ref() << "Damage", 0);
			renderer.heatmap(recorder_.worms[i].damageHm);
		});
	}

	gfx->playRenderer.pal = common.exepal;
}
