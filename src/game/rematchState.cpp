#include "rematchState.hpp"

#include "gfx.hpp"
#include "game.hpp"
#include "inputState.hpp"
#include "gamePlayState.hpp"
#include "fileSelectorState.hpp"
#include "filesystem.hpp"
#include "sfx.hpp"
#include "keys.hpp"
#include "net/session.hpp"
#include "controller/controller.hpp"

RematchState::RematchState(Game& lastGame)
: lastGame_(lastGame)
{
}

void RematchState::enter()
{
	fill(gfx->playRenderer.bmp, 0);
	gfx->clearKeys();

	prevRandomLevel_ = gfx->settings->randomLevel;
	prevLevelFile_ = gfx->settings->levelFile;
}

void RematchState::handleEvent(SDL_Event& ev)
{
	gfx->processEvent(ev);
}

bool RematchState::update()
{
	if (!gfx->netSession)
		return false;

	gfx->netSession->update();

	auto state = gfx->netSession->sessionState();

	// Rematch game is starting — transfer controller and enter gameplay
	if (state == NetSession::Playing)
	{
		auto ctrl = gfx->netSession->releaseController();
		gfx->controller = std::unique_ptr<Controller>(ctrl.release());
		gfx->stateStack.scheduleReplaceTop(std::make_unique<GamePlayState>());
		return true;
	}

	if (state == NetSession::Disconnected || state == NetSession::Failed)
	{
		gfx->netSession.reset();
		gfx->stateStack.scheduleReplaceTop(
			std::make_unique<InfoBoxState>("PEER DISCONNECTED", 160, 100, true));
		return true;
	}

	// Check if a level selector was open and just closed
	if (levelSelectorOpen_)
	{
		// LevelSelectorState was pushed on top of us; if we're updating,
		// it has already popped. Check if settings changed.
		levelSelectorOpen_ = false;

		bool changed = (gfx->settings->randomLevel != prevRandomLevel_) ||
		               (gfx->settings->levelFile != prevLevelFile_);

		if (changed && gfx->netSession->isHost())
		{
			gfx->netSession->setRematchLevel(
				gfx->settings->randomLevel, gfx->settings->levelFile);
		}

		prevRandomLevel_ = gfx->settings->randomLevel;
		prevLevelFile_ = gfx->settings->levelFile;
	}

	// Handle input
	bool isHost = gfx->netSession->isHost();

	// Escape = disconnect
	if (gfx->testSDLKeyOnce(SDL_SCANCODE_ESCAPE) ||
	    gfx->testGamepadButtonOnce(SDL_GAMEPAD_BUTTON_EAST))
	{
		gfx->netSession->disconnect();
		gfx->netSession.reset();
		fill(gfx->playRenderer.bmp, 0);
		fill(gfx->singleScreenRenderer.bmp, 0);
		return false;
	}

	// Enter/Fire = toggle ready
	if (gfx->testSDLKeyOnce(SDL_SCANCODE_RETURN) ||
	    gfx->testControlOnce(WormSettingsExtensions::Fire) ||
	    gfx->testGamepadButtonOnce(SDL_GAMEPAD_BUTTON_SOUTH))
	{
		gfx->netSession->toggleReady();
	}

	// Host: open level selector
	if (isHost &&
	    (gfx->testSDLKeyOnce(SDL_SCANCODE_TAB) ||
	     gfx->testControlOnce(WormSettingsExtensions::Change) ||
	     gfx->testGamepadButtonOnce(SDL_GAMEPAD_BUTTON_NORTH)))
	{
		levelSelectorOpen_ = true;
		gfx->stateStack.push(std::make_unique<LevelSelectorState>(), gfx);
	}

	return true;
}

std::string RematchState::levelDisplayName() const
{
	if (gfx->settings->randomLevel || gfx->settings->levelFile.empty())
		return "RANDOM";
	return getBasename(getLeaf(gfx->settings->levelFile));
}

void RematchState::draw()
{
	Common& common = *gfx->common;
	Font& font = common.font;

	gfx->playRenderer.pal = common.exepal;
	fill(gfx->playRenderer.bmp, 0);

	int cx = 160;
	int y = 40;

	// Title
	std::string title = "REMATCH";
	int tw = font.getDims(title);
	font.drawText(gfx->playRenderer.bmp, title, cx - tw / 2, y, 50);
	y += 16;

	// Score summary from last game
	{
		Worm* w0 = lastGame_.wormByIdx(0);
		Worm* w1 = lastGame_.wormByIdx(1);
		if (w0 && w1)
		{
			std::string score = w0->settings->name + "  " +
			                    std::to_string(w0->kills) + " - " +
			                    std::to_string(w1->kills) + "  " +
			                    w1->settings->name;
			int sw = font.getDims(score);
			font.drawText(gfx->playRenderer.bmp, score, cx - sw / 2, y, 7);
		}
		y += 16;
	}

	// Level
	std::string levelLine = "LEVEL: " + levelDisplayName();
	int lw = font.getDims(levelLine);
	font.drawText(gfx->playRenderer.bmp, levelLine, cx - lw / 2, y, 7);
	y += 16;

	if (!gfx->netSession)
		return;

	bool isHost = gfx->netSession->isHost();

	// Ready indicators
	{
		bool localReady = gfx->netSession->localReady();
		bool remoteReady = gfx->netSession->remoteReady();

		std::string you = isHost ? "HOST (YOU)" : "CLIENT (YOU)";
		std::string peer = isHost ? "CLIENT" : "HOST";

		std::string localLine = you + ": " + (localReady ? "READY" : "NOT READY");
		std::string remoteLine = peer + ": " + (remoteReady ? "READY" : "NOT READY");

		int localColor = localReady ? 63 : 18;
		int remoteColor = remoteReady ? 63 : 18;

		int llw = font.getDims(localLine);
		font.drawText(gfx->playRenderer.bmp, localLine, cx - llw / 2, y, localColor);
		y += 10;

		int rlw = font.getDims(remoteLine);
		font.drawText(gfx->playRenderer.bmp, remoteLine, cx - rlw / 2, y, remoteColor);
		y += 20;
	}

	// Instructions
	{
		std::string readyHint = "ENTER/FIRE = TOGGLE READY";
		int rw = font.getDims(readyHint);
		font.drawText(gfx->playRenderer.bmp, readyHint, cx - rw / 2, y, 6);
		y += 10;

		if (isHost)
		{
			std::string levelHint = "TAB/CHANGE = CHANGE LEVEL";
			int chw = font.getDims(levelHint);
			font.drawText(gfx->playRenderer.bmp, levelHint, cx - chw / 2, y, 6);
			y += 10;
		}

		std::string escHint = "ESC = DISCONNECT";
		int ew = font.getDims(escHint);
		font.drawText(gfx->playRenderer.bmp, escHint, cx - ew / 2, y, 6);
	}
}
