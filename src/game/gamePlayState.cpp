#include "gamePlayState.hpp"

#include "gfx.hpp"
#include "statsState.hpp"
#include "inputState.hpp"
#include "controller/controller.hpp"
#include "stats_recorder.hpp"
#include "game.hpp"

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

		// Game ended — show stats if available and game actually finished
		Game* game = gfx->controller->currentGame();
		if (game && game->statsRecorder)
		{
			auto* stats = dynamic_cast<NormalStatsRecorder*>(game->statsRecorder.get());
			if (stats && stats->gameTime > 0)
			{
				gfx->stateStack.scheduleReplaceTop(
					std::make_unique<StatsState>(*stats, *game));
				return false;
			}
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
