#include "localController.hpp"

#include <chrono>
#include "../keys.hpp"
#include "../gfx.hpp"
#include "../sfx.hpp"
#include "../reader.hpp"
#include "../filesystem.hpp"

#include "../ai/predictive_ai.hpp"
#include "../worm.hpp"
#include "../spectatorviewport.hpp"
#include "../viewport.hpp"

#include <cctype>

std::shared_ptr<WormAI> createAi(int controller, Worm& worm, Settings& settings)
{
	if (controller == 1)
		return std::shared_ptr<WormAI>(new DumbLieroAI());
	else if (controller == 2)
		return std::shared_ptr<WormAI>(new FollowAI(
			Weights(), settings.aiParallels, worm.index == 0));

	return std::shared_ptr<WormAI>();
}

LocalController::LocalController(std::shared_ptr<Common> common, std::shared_ptr<Settings> settings)
: game(common, settings, std::shared_ptr<SoundPlayer>(new DefaultSoundPlayer(*common)))
, state(StateInitial)
, fadeValue(0)
, goingToMenu(false)
{
	auto worm1 = std::make_shared<Worm>();
	worm1->settings = settings->wormSettings[0];
	worm1->health = worm1->settings->health;
	worm1->index = 0;
	worm1->statsX = 0;
	worm1->ai = createAi(worm1->settings->controller, *worm1, *settings);

	auto worm2 = std::make_shared<Worm>();
	worm2->settings = settings->wormSettings[1];
	worm2->health = worm2->settings->health;
	worm2->index = 1;
	worm2->statsX = 218;
	worm2->ai = createAi(worm2->settings->controller, *worm2, *settings);

	game.addViewport(new Viewport(Rect(0, 0, 158, 158), worm1->index, 504, 350));
	game.addViewport(new Viewport(Rect(160, 0, 158+160, 158), worm2->index, 504, 350));

	game.addWorm(worm1);
	game.addWorm(worm2);

	// +68 on x to align the viewport in the middle
	game.addSpectatorViewport(new SpectatorViewport(Rect(0, 0, 504 + 68, 350), 504, 350));
}

LocalController::~LocalController()
{
	endRecord();
}

void LocalController::onKey(int key, bool keyState)
{
	Worm::Control control;
	Worm* worm = game.findControlForKey(key, control);
	if(worm)
	{
		worm->cleanControlStates.set(control, keyState);

		if(control < Worm::MaxControl)
		{
			// Only real controls
			worm->setControlState(control, keyState);
		}

		if(worm->cleanControlStates[WormSettings::Dig])
		{
			worm->press(Worm::Left);
			worm->press(Worm::Right);
		}
		else
		{
			if(!worm->cleanControlStates[Worm::Left])
				worm->release(Worm::Left);
			if(!worm->cleanControlStates[Worm::Right])
				worm->release(Worm::Right);
		}
	}

	if(key == DkEscape && !goingToMenu)
	{
		fadeValue = 31;
		goingToMenu = true;
	}
}

// Called when the controller loses focus. When not focused, it will not receive key events among other things.
void LocalController::unfocus()
{
	if(replay.get())
		replay->unfocus();
	if(state == StateWeaponSelection)
		ws->unfocus();
}

// Called when the controller gets focus.
void LocalController::focus()
{
	if(state == StateGameEnded)
	{
		goingToMenu = true;
		fadeValue = 0;
		return;
	}
	if(state == StateWeaponSelection)
		ws->focus();
	if(replay.get())
		replay->focus();
	if(state == StateInitial)
		changeState(StateWeaponSelection);
	game.focus(gfx.playRenderer);
	// FIXME rewrite the focus function to avoid nonsense like this?
	game.focus(gfx.singleScreenRenderer);
	goingToMenu = false;
	fadeValue = 0;
}

bool LocalController::process()
{
	if(state == StateWeaponSelection)
	{
		// Apply key repeat for held keys during weapon selection.
		// Without SDL repeat events, we need to re-set control bits that
		// were consumed by WeaponSelection (via release/pressedOnce).
		for (std::size_t wi = 0; wi < game.worms.size(); ++wi) {
			Worm& worm = *game.worms[wi];
			for (int bit = 0; bit < 7; ++bit) {
				bool held = worm.cleanControlStates[bit];
				if (held) {
					if (!worm.controlStates[bit]) {
						// Key is physically held but bit was consumed — apply repeat logic
						++wormHeldFrames[wi][bit];
						if (wormHeldFrames[wi][bit] >= KEY_REPEAT_INITIAL &&
						    (wormHeldFrames[wi][bit] - KEY_REPEAT_INITIAL) % KEY_REPEAT_INTERVAL == 0) {
							worm.press(static_cast<Worm::Control>(bit));
						}
					} else {
						// Bit is set (initial press frame or just re-set) — reset counter
						wormHeldFrames[wi][bit] = 0;
					}
				} else {
					wormHeldFrames[wi][bit] = 0;
				}
			}
		}

		if(ws->processFrame())
			changeState(StateGame);
	}
	else if(state == StateGame || state == StateGameEnded)
	{
		int realFrameSkip = inverseFrameSkip ? !(cycles % frameSkip) : frameSkip;
		for(int i = 0; i < realFrameSkip && (state == StateGame || state == StateGameEnded); ++i)
		{
			int phase = game.cycles % 2;
			for (std::size_t i = 0; i < game.worms.size(); ++i)
			{
				Worm& worm = *game.worms[(i + phase) % game.worms.size()];
				if(worm.ai.get())
				{
					auto start_time = std::chrono::steady_clock::now();
					worm.ai->process(game, worm);
					auto time = std::chrono::steady_clock::now() - start_time;
					game.statsRecorder->aiProcessTime(&worm, time);
				}
			}
			if(replay.get())
			{
				try
				{
					replay->recordFrame();
				}
				catch(std::runtime_error& e)
				{
					Console::writeWarning(std::string("Error recording replay frame: ") + e.what());
					Console::writeWarning("Replay recording aborted");
					replay.reset();
				}
			}
			game.processFrame();

			if(game.isGameOver())
			{
				changeState(StateGameEnded);
			}
		}
	}

	//CommonController::process();

	if(goingToMenu)
	{
		if(fadeValue > 0)
			fadeValue -= 1;
		else
		{
			if(state == StateGameEnded)
			{
				endRecord();
				game.statsRecorder->finish(game);
			}
			return false;
		}
	}
	else
	{
		if(fadeValue < 33)
		{
			fadeValue += 1;
		}
	}

	return true;
}

void LocalController::draw(Renderer& renderer, bool useSpectatorViewports)
{
	if(state == StateWeaponSelection)
	{
		ws->draw(renderer, state, useSpectatorViewports);
	}
	else if(state == StateGame || state == StateGameEnded || state == StateInitial)
	{
		game.draw(renderer, state, useSpectatorViewports);
	}
	renderer.fadeValue = fadeValue;
}

void LocalController::changeState(GameState newState)
{
	if(state == newState)
		return;

	// NOTE: We prepare new state before destroying the old.
	// e.g. weapon selection is destroyed first after we successfully
	// started recording.

	// NOTE: Must do this here before starting recording!
	if(state == StateWeaponSelection)
	{
		ws->finalize();
	}

	if(newState == StateWeaponSelection)
	{
		ws.reset(new WeaponSelection(game));
	}
	else if(newState == StateGame)
	{
		// NOTE: This must be done before the replay recording starts below
		for(std::size_t i = 0; i < game.worms.size(); ++i)
		{
			Worm& worm = *game.worms[i];
			worm.lives = game.settings->lives;
		}

		if(game.settings->extensions && game.settings->recordReplays)
		{
			try
			{
				std::time_t ticks = std::time(0);
				std::tm* now = std::localtime(&ticks);

				char buf[512];
				std::strftime(buf, sizeof(buf), "%Y-%m-%d %H.%M.%S", now);

				std::string playerNames = " ";
				for(std::size_t i = 0; i < 2; ++i)
				{
					Worm& worm = *game.worms[i];
					std::string const& name = worm.settings->name;
					int chars = 0;

					if(i > 0)
						playerNames.push_back('-');
					for(std::size_t c = 0; c < name.size() && chars < 4; ++c, ++chars)
					{
						unsigned char ch = (unsigned char)name[c];
						if(std::isalnum(ch))
							playerNames.push_back(ch);
					}
				}

				auto node = gfx.getConfigNode() / "Replays" / (buf + playerNames + ".lrp");

				replay.reset(new ReplayWriter(node.toWriter()));

				replay->beginRecord(game);
			}
			catch(std::runtime_error& e)
			{
				gfx.pendingErrorMessage = std::string("Error starting replay recording: ") + e.what();
				goingToMenu = true;
				fadeValue = 0;
				return;
			}
		}

		game.startGame();
	}
	else if(newState == StateGameEnded)
	{
		if(!goingToMenu)
		{
			fadeValue = 180;
			goingToMenu = true;
		}
	}

	if(state == StateWeaponSelection)
	{
		fadeValue = 33;
		ws.reset();
	}

	state = newState;
}

void LocalController::endRecord()
{
	if(replay.get())
	{
		replay.reset();
	}
}

void LocalController::swapLevel(Level& newLevel)
{
	currentLevel()->swap(newLevel);
}

Level* LocalController::currentLevel()
{
	return &game.level;
}

Game* LocalController::currentGame()
{
	return &game;
}

bool LocalController::running()
{
	return state != StateGameEnded && state != StateInitial;
}
