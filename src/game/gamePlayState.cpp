#include "gamePlayState.hpp"

#include "gfx.hpp"
#include "statsState.hpp"
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
		// Game ended — show stats if available
		Game* game = gfx->controller->currentGame();
		if (game && game->statsRecorder)
		{
			auto* stats = dynamic_cast<NormalStatsRecorder*>(game->statsRecorder.get());
			if (stats)
			{
				gfx->stateStack.replaceTop(
					std::make_unique<StatsState>(*stats, *game), gfx);
				return true;
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
