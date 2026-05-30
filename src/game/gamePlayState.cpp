#include "gamePlayState.hpp"

#include <cstdio>
#include "gfx.hpp"
#include "statsState.hpp"
#include "inputState.hpp"
#include "controller/controller.hpp"
#include "stats_recorder.hpp"
#include "game.hpp"
#include "net/session.hpp"

void GamePlayState::enter()
{
	gfx->controller->focus();
}

void GamePlayState::handleEvent(SDL_Event& ev)
{
	gfx->processEvent(ev, gfx->controller.get());
}

bool GamePlayState::update()
{
	// Poll network session if active
	if (gfx->netSession)
	{
		gfx->netSession->update();
		auto state = gfx->netSession->sessionState();
		if (state == NetSession::Disconnected || state == NetSession::Failed)
		{
			gfx->controller->markUnresumable();
			gfx->netSession.reset();
			gfx->stateStack.scheduleReplaceTop(
				std::make_unique<InfoBoxState>("PEER DISCONNECTED", 320/2, 200/2, true));
			return false;
		}
		if (gfx->netSession->desyncDetected())
		{
			uint32_t frame = gfx->netSession->desyncFrame();
			char msg[64];
			snprintf(msg, sizeof(msg), "DESYNC AT FRAME %u", frame);
			gfx->controller->markUnresumable();
			gfx->netSession.reset();
			gfx->stateStack.scheduleReplaceTop(
				std::make_unique<InfoBoxState>(msg, 320/2, 200/2, true));
			return false;
		}
	}

	if (!gfx->controller->process())
	{
		// Check for pending error message
		if (!gfx->pendingErrorMessage.empty())
		{
			std::string msg = std::move(gfx->pendingErrorMessage);
			gfx->pendingErrorMessage.clear();
			gfx->stateStack.scheduleReplaceTop(
				std::make_unique<InfoBoxState>(std::move(msg), 320/2, 200/2, true));
			return false;
		}

		// Game ended — show stats if available and game actually finished.
		// statsGame() is the live game for single-player/replay and the
		// shadow Game for rollback multiplayer (live's recorder is a
		// no-op there — see RollbackController::setupShadowGame).
		Game* game = gfx->controller->statsGame();
		if (game && game->statsRecorder)
		{
			auto* stats = dynamic_cast<NormalStatsRecorder*>(game->statsRecorder.get());
			if (stats && stats->gameTime > 0)
			{
				bool isMultiplayer = gfx->netSession != nullptr;

				// Transition network session to rematch state to keep connection alive
				if (isMultiplayer)
					gfx->netSession->enterRematch();

				gfx->stateStack.scheduleReplaceTop(
					std::make_unique<StatsState>(*stats, *game, isMultiplayer));
				return false;
			}
		}

		// Clear framebuffer so menu doesn't capture stale overlay content
		if (gfx->netSession) {
			gfx->netSession.reset();
			fill(gfx->playRenderer.bmp, 0);
			fill(gfx->singleScreenRenderer.bmp, 0);
		}
		return false;
	}

	return true;
}

void GamePlayState::draw()
{
	gfx->playRenderer.clear();
	gfx->controller->draw(gfx->playRenderer, false);

	gfx->singleScreenRenderer.clear();
	gfx->controller->draw(gfx->singleScreenRenderer, true);
}
