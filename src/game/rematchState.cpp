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
, menu_(true)  // centered
{
}

void RematchState::enter()
{
	fill(gfx->playRenderer.bmp, 0);
	gfx->clearKeys();

	prevRandomLevel_ = gfx->settings->randomLevel;
	prevLevelFile_ = gfx->settings->levelFile;

	// Build menu items
	bool isHost = gfx->netSession && gfx->netSession->isHost();

	menu_.addItem(MenuItem(48, 7, "LEVEL", RmLevel));
	menu_.addItem(MenuItem(48, 7, "READY", RmReady));
	menu_.addItem(MenuItem(48, 7, "DISCONNECT", RmDisconnect));

	// Only host can select the level item
	if (!isHost)
		menu_.items[0].selectable = false;

	menu_.valueOffsetX = 80;
	menu_.place(120, 90);
	menu_.moveToId(RmReady);
}

void RematchState::handleEvent(SDL_Event& ev)
{
	gfx->processEvent(ev);
}

void RematchState::updateMenuItems()
{
	if (!gfx->netSession)
		return;

	// Update level display
	auto* levelItem = menu_.itemFromId(RmLevel);
	if (levelItem)
	{
		levelItem->hasValue = true;
		levelItem->value = '"' + levelDisplayName() + '"';
	}

	// Update ready item text
	auto* readyItem = menu_.itemFromId(RmReady);
	if (readyItem)
	{
		bool localReady = gfx->netSession->localReady();
		readyItem->string = localReady ? "NOT READY" : "READY";
	}
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

	Common& common = *gfx->common;

	// Menu navigation
	if (gfx->testSDLKeyOnce(SDL_SCANCODE_UP)
	 || gfx->testControlOnce(WormSettingsExtensions::Up)
	 || gfx->testGamepadDirOnce(SDL_GAMEPAD_BUTTON_DPAD_UP))
	{
		sfx.play(common, 26);
		menu_.movement(-1);
	}

	if (gfx->testSDLKeyOnce(SDL_SCANCODE_DOWN)
	 || gfx->testControlOnce(WormSettingsExtensions::Down)
	 || gfx->testGamepadDirOnce(SDL_GAMEPAD_BUTTON_DPAD_DOWN))
	{
		sfx.play(common, 25);
		menu_.movement(1);
	}

	// Escape = disconnect
	if (gfx->testSDLKeyOnce(SDL_SCANCODE_ESCAPE)
	 || gfx->testControlOnce(WormSettingsExtensions::Jump)
	 || gfx->testGamepadButtonOnce(SDL_GAMEPAD_BUTTON_EAST))
	{
		gfx->netSession->disconnect();
		gfx->netSession.reset();
		fill(gfx->playRenderer.bmp, 0);
		fill(gfx->singleScreenRenderer.bmp, 0);
		return false;
	}

	// Enter/Fire = activate selected item
	if (gfx->testSDLKeyOnce(SDL_SCANCODE_RETURN)
	 || gfx->testSDLKeyOnce(SDL_SCANCODE_KP_ENTER)
	 || gfx->testControlOnce(WormSettingsExtensions::Fire)
	 || gfx->testGamepadButtonOnce(SDL_GAMEPAD_BUTTON_SOUTH))
	{
		int sel = menu_.selectedId();
		switch (sel)
		{
			case RmLevel:
				// Host opens level selector
				sfx.play(common, 27);
				levelSelectorOpen_ = true;
				gfx->stateStack.push(std::make_unique<LevelSelectorState>(), gfx);
				break;

			case RmReady:
				sfx.play(common, 27);
				gfx->netSession->toggleReady();
				break;

			case RmDisconnect:
				sfx.play(common, 27);
				gfx->netSession->disconnect();
				gfx->netSession.reset();
				fill(gfx->playRenderer.bmp, 0);
				fill(gfx->singleScreenRenderer.bmp, 0);
				return false;
		}
	}

	updateMenuItems();
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
	y += 14;

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
		y += 14;
	}

	// Peer ready status
	if (gfx->netSession)
	{
		bool isHost = gfx->netSession->isHost();
		bool remoteReady = gfx->netSession->remoteReady();

		std::string peer = isHost ? "CLIENT" : "HOST";
		std::string remoteLine = peer + ": " + (remoteReady ? "READY" : "NOT READY");
		int remoteColor = remoteReady ? 63 : 18;

		int rlw = font.getDims(remoteLine);
		font.drawText(gfx->playRenderer.bmp, remoteLine, cx - rlw / 2, y, remoteColor);
	}

	// Draw menu
	menu_.draw(common, gfx->playRenderer, false);
}
