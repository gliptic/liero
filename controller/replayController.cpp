#include "replayController.hpp"

#include "../game.hpp"
#include "stats_presenter.hpp"
#include "../spectatorviewport.hpp"
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
	game->focus(gfx.playRenderer);
	game->focus(gfx.singleScreenRenderer);
	goingToMenu = false;
	fadeValue = 0;
}

bool ReplayController::process()
{
	if(state == StateGame || state == StateGameEnded)
	{
		if(gfx.testSDLKeyOnce(SDL_SCANCODE_R))
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
					if (!replay->playbackFrame(*gfx.primaryRenderer))
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

void ReplayController::draw(Renderer& renderer, bool useSpectatorViewports)
{
	if(state == StateGame || state == StateGameEnded)
	{
		game->draw(renderer, useSpectatorViewports, true);
	}
	renderer.fadeValue = fadeValue;
}

void ReplayController::changeState(State newState)
{
	if(state == newState)
		return;

	if(newState == StateGame)
	{
		// FIXME: the viewports are changed based on the replay for some
		// reason, so we need to restore them here. Probably makes more sense
		// to not save the viewports at all. But that probably breaks save
		// format compatibility?
		game->clearViewports();

		// for backwards compatibility reasons, this is not stored within the
		// replay. Yet.
		game->worms[0]->statsX = 0;
		game->worms[1]->statsX = 218;

		// spectator viewport is always full size
		// +68 on x to align the viewport in the middle
		game->addSpectatorViewport(new SpectatorViewport(gvl::rect(0, 0, 504 + 68, 350), 504, 350));
		if (gfx.settings->singleScreenReplay)
		{
			// on single screen replay, use the spectator viewport for the
			// main screen as well
			// we can't use the same object, as the vector's clean function will delete them
			game->addViewport(new SpectatorViewport(gvl::rect(0, 0, 504 + 68, 350), 504, 350));
		}
		else
		{
			game->addViewport(new Viewport(gvl::rect(0, 0, 158, 158), game->worms[0]->index, 504, 350));
			game->addViewport(new Viewport(gvl::rect(160, 0, 158+160, 158), game->worms[1]->index, 504, 350));
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
