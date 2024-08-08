#include "replayController.hpp"

#include "../game.hpp"
#include "stats_presenter.hpp"
#include "../viewport.hpp"
#include "../sfx.hpp"

ReplayController::ReplayController(
	gvl::shared_ptr<Common> common, gvl::source source)
: state(StateInitial)
, fadeValue(0)
, goingToMenu(false)
, replay(new ReplayReader(source))
, common(common)
{
}

void ReplayController::onKey(int key, bool keyState)
{
	if(key == DkEscape && !goingToMenu)
	{
		fadeValue = 31;
		goingToMenu = true;
	}
}

// Called when the controller loses focus. When not focused, it will not receive key events among other things.
void ReplayController::unfocus()
{
}

// Called when the controller gets focus.
void ReplayController::focus()
{
	if(state == StateGameEnded)
	{
		goingToMenu = true;
		fadeValue = 0;
		return;
	}
	if(state == StateInitial)
	{
		try
		{
			game = replay->beginPlayback(common, gvl::shared_ptr<SoundPlayer>(new DefaultSoundPlayer(*common)));
		}
		catch(std::runtime_error& e)
		{
			gfx.infoBox(std::string("Error starting replay playback: ") + e.what());
			goingToMenu = true;
			fadeValue = 0;
			return;
		}
		// Changing state first when game is available
		changeState(StateGame);
	}
	game->focus(gfx);
	goingToMenu = false;
	fadeValue = 0;
}

bool ReplayController::process()
{
	if(state == StateGame || state == StateGameEnded)
	{
		if(gfx.testSDLKeyOnce(SDLK_r))
		{
			*game = *initialGame;
			game->postClone(*initialGame, true);
			replay->reader = initialReader;
		}

		int realFrameSkip = inverseFrameSkip ? !(cycles % frameSkip) : frameSkip;
		for(int i = 0; i < realFrameSkip && (state == StateGame || state == StateGameEnded); ++i)
		{
			if(replay.get())
			{
				try
				{
					if(!replay->playbackFrame(gfx))
					{
						// End of replay
						replay.reset();
					}
				}
				catch(gvl::stream_error& e)
				{
					gfx.infoBox(std::string("Stream error in replay: ") + e.what());
					changeState(StateGameEnded);
					replay.reset();
				}
				catch(gvl::archive_check_error& e)
				{
					gfx.infoBox(std::string("Archive error in replay: ") + e.what());
					changeState(StateGameEnded);
					replay.reset();
				}
			}
			game->processFrame();

			if(goingToMenu)
			{
				if(fadeValue > 0)
					fadeValue -= 1;
				else
					break;
			}
			else if(fadeValue < 33)
			{
				fadeValue += 1;
			}
		}

		if(game->isGameOver())
		{
			changeState(StateGameEnded);
		}
	}

	CommonController::process();

	if (goingToMenu && fadeValue <= 0)
	{
		if (state == StateGameEnded)
		{
			game->statsRecorder->finish(*game);
			presentStats(static_cast<NormalStatsRecorder&>(*game->statsRecorder), *game);
		}
		return false;
	}

	if (!replay.get() && state == StateGame)
	{
		game->statsRecorder->finish(*game);
		presentStats(static_cast<NormalStatsRecorder&>(*game->statsRecorder), *game);
		return false;
	}

	return true;
}

void ReplayController::draw(Renderer& renderer)
{
	if(state == StateGame || state == StateGameEnded)
	{
		game->draw(renderer, true);
	}
	renderer.fadeValue = fadeValue;
}

void ReplayController::changeState(State newState)
{
	if(state == newState)
		return;

	if(newState == StateGame)
	{
		if (gfx.settings->singleScreenReplay) {
			game->clearViewports();
			// +68 on x to align the viewport in the middle
			game->addViewport(new Viewport(gvl::rect(0, 0, 504 + 68, 350), game->worms[0]->index, 0, 504, 350));
			// TODO: a bit weird to duplicate this, but it's needed to draw health bars etc
			game->addViewport(new Viewport(gvl::rect(0, 0, 504 + 68, 350), game->worms[1]->index, 538, 504, 350));
		}
		game->startGame();
#if !ENABLE_TRACING
		initialGame.reset(new Game(*game));
		initialGame->postClone(*game, true);
#endif
		initialReader = replay->reader;
	}
	else if(newState == StateGameEnded)
	{
		if(!goingToMenu)
		{
			fadeValue = 180;
			goingToMenu = true;
		}
	}

	state = newState;
}

void ReplayController::swapLevel(Level& newLevel)
{
	currentLevel()->swap(newLevel);
}

Level* ReplayController::currentLevel()
{
	if(game.get() && replay.get())
		return &game->level;
	return 0;
}

Game* ReplayController::currentGame()
{
	return game.get();
}


bool ReplayController::running()
{
	return state != StateGameEnded && state != StateInitial;
}
