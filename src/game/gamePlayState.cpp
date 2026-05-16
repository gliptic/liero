#include "gamePlayState.hpp"

#include "gfx.hpp"
#include "controller/controller.hpp"

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
		return false;

	return true;
}

void GamePlayState::draw()
{
	gfx->playRenderer.clear();
	gfx->controller->draw(gfx->playRenderer, false);

	gfx->singleScreenRenderer.clear();
	gfx->controller->draw(gfx->singleScreenRenderer, true);
}
