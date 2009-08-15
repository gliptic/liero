#ifndef UUID_9CD8C22BC14D4832AE2A859530FE6339
#define UUID_9CD8C22BC14D4832AE2A859530FE6339

struct Controller
{
	virtual ~Controller()
	{
	}
	
	// Called when a key event is forwarded to the controller
	virtual void onKey(int key, bool state) = 0;
	
	// Called when the controller loses focus. When not focused, it will not receive key events among other things.
	virtual void unfocus() = 0;
	
	// Called when the controller gets focus.
	virtual void focus() = 0;
	
	virtual bool process() = 0;
	
	virtual void draw() = 0;
	
	// Returns true if the game is still running. The menu should check this to decide whether to show the resume option.
	virtual bool running() = 0;
	
	virtual Level* currentLevel() = 0;
	
	virtual Game* currentGame() = 0;
	
	virtual void swapLevel(Level& newLevel) = 0;
};

#include "game.hpp"
#include "worm.hpp"
#include "weapsel.hpp"
#include "replay.hpp"
#include "console.hpp"
#include <gvl/serialization/except.hpp>
#include <gvl/io/stream.hpp>
#include <gvl/io/devnull.hpp>
#include <gvl/io/fstream.hpp>
#include <ctime>

struct CommonController : Controller
{
	CommonController()
	: frameSkip(1)
	, inverseFrameSkip(false)
	, cycles(0)
	{
	}
	
	bool process()
	{
		int newFrameSkip = 0;
		if(gfx.testSDLKeyOnce(SDLK_1))
			newFrameSkip = 1;
		else if(gfx.testSDLKeyOnce(SDLK_2))
			newFrameSkip = 2;
		else if(gfx.testSDLKeyOnce(SDLK_3))
			newFrameSkip = 4;
		else if(gfx.testSDLKeyOnce(SDLK_4))
			newFrameSkip = 8;
		else if(gfx.testSDLKeyOnce(SDLK_5))
			newFrameSkip = 16;
		else if(gfx.testSDLKeyOnce(SDLK_6))
			newFrameSkip = 32;
		else if(gfx.testSDLKeyOnce(SDLK_7))
			newFrameSkip = 64;
		else if(gfx.testSDLKeyOnce(SDLK_8))
			newFrameSkip = 128;
		else if(gfx.testSDLKeyOnce(SDLK_9))
			newFrameSkip = 256;
		else if(gfx.testSDLKeyOnce(SDLK_0))
			newFrameSkip = 512;
			
		if(newFrameSkip)
		{
			inverseFrameSkip = (gfx.testSDLKey(SDLK_RCTRL) || gfx.testSDLKey(SDLK_LCTRL));
			frameSkip = newFrameSkip;
		}
		
		++cycles;
			
		return true;
	}
	
	int frameSkip;
	bool inverseFrameSkip;
	int cycles;
};

struct LocalController : CommonController
{
	enum State
	{
		StateInitial,
		StateWeaponSelection,
		StateGame,
		StateGameEnded
	};
	
	LocalController(gvl::shared_ptr<Common> common, gvl::shared_ptr<Settings> settings)
	: game(common, settings)
	, state(StateInitial)
	, fadeValue(0)
	, goingToMenu(false)
	{
		game.createDefaults();
	}
	
	~LocalController()
	{
		endRecord();
	}
	
	void onKey(int key, bool keyState)
	{
		Worm::Control control;
		Worm* worm = game.findControlForKey(key, control);
		if(worm)
		{
			worm->setControlState(control, keyState);
		}
				
		if(key == DkEscape && !goingToMenu)
		{
			fadeValue = 31;
			goingToMenu = true;
		}
	}
	
	// Called when the controller loses focus. When not focused, it will not receive key events among other things.
	void unfocus()
	{
		if(replay.get())
			replay->unfocus();
		if(state == StateWeaponSelection)
			ws->unfocus();
	}
	
	// Called when the controller gets focus.
	void focus()
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
		if(state == StateInitial) // TODO: Have a separate initial state
			changeState(StateWeaponSelection);
		game.focus();
		goingToMenu = false;
		fadeValue = 0;
	}
	
	bool process()
	{
		if(state == StateWeaponSelection)
		{
			if(ws->processFrame())
				changeState(StateGame);
		}
		else if(state == StateGame || state == StateGameEnded)
		{
			int realFrameSkip = inverseFrameSkip ? !(cycles % frameSkip) : frameSkip;
			for(int i = 0; i < realFrameSkip && (state == StateGame || state == StateGameEnded); ++i)
			{
				for(std::size_t i = 0; i < game.worms.size(); ++i)
				{
					Worm& worm = *game.worms[i];
					if(worm.ai.get())
						worm.ai->process();
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
					endRecord();
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
	
	void draw()
	{
		if(state == StateWeaponSelection)
		{
			ws->draw();
		}
		else if(state == StateGame || state == StateGameEnded || state == StateInitial)
		{
			game.draw();
		}
		gfx.fadeValue = fadeValue;
	}
	
	void changeState(State newState)
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
			
			game.startGame();
			if(game.settings->extensions && game.settings->recordReplays)
			{
				try
				{
					std::time_t ticks = std::time(0);
					std::tm* now = std::localtime(&ticks);
					
					char buf[512];
					std::strftime(buf, sizeof(buf), " %Y-%m-%d %H.%M.%S.lrp", now);
					
					std::string prefix;
					for(std::size_t i = 0; i < 2; ++i)
					{
						Worm& worm = *game.worms[i];
						std::string const& name = worm.settings->name;
						int chars = 0;
						
						if(i > 0)
							prefix.push_back('-');
						for(std::size_t c = 0; c < name.size() && chars < 4; ++c, ++chars)
						{
							unsigned char ch = (unsigned char)name[c];
							if(std::isalnum(ch))
								prefix.push_back(ch);
						}
						
						
					}
					std::string path = joinPath(lieroEXERoot, prefix + buf);
					replay.reset(new ReplayWriter(gvl::stream_ptr(new gvl::fstream(path.c_str(), "wb"))));
					replay->beginRecord(game);
				}
				catch(std::runtime_error& e)
				{
					//Console::writeWarning();
					gfx.infoBox(std::string("Error starting replay recording: ") + e.what());
					goingToMenu = true;
					fadeValue = 0;
					return;
				}
			}
			
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
	
	void endRecord()
	{
		if(replay.get())
		{
			replay.reset();
		}
	}
		
	void swapLevel(Level& newLevel)
	{
		currentLevel()->swap(newLevel);
	}
	
	Level* currentLevel()
	{
		return &game.level;
	}
	
	Game* currentGame()
	{
		return &game;
	}
		
	bool running()
	{
		return state != StateGameEnded && state != StateInitial;
	}
	
	Game game;
	std::auto_ptr<WeaponSelection> ws;
	State state;
	int fadeValue;
	bool goingToMenu;
	std::auto_ptr<ReplayWriter> replay;
};

struct ReplayController : CommonController
{
	enum State
	{
		StateInitial,
		StateGame,
		StateGameEnded
	};
	
	ReplayController(gvl::shared_ptr<Common> common, gvl::stream_ptr source)
	: state(StateInitial)
	, fadeValue(0)
	, goingToMenu(false)
	, replay(new ReplayReader(source))
	, common(common)
	{
	}
	
	void onKey(int key, bool keyState)
	{
		if(key == DkEscape && !goingToMenu)
		{
			fadeValue = 31;
			goingToMenu = true;
		}
	}
	
	// Called when the controller loses focus. When not focused, it will not receive key events among other things.
	void unfocus()
	{
	}
	
	// Called when the controller gets focus.
	void focus()
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
				game = replay->beginPlayback(common);
			}
			catch(std::runtime_error& e)
			{
				//Console::writeWarning(std::string("Error starting replay playback: ") + e.what());
				gfx.infoBox(std::string("Error starting replay playback: ") + e.what());
				goingToMenu = true;
				fadeValue = 0;
				return;
			}
			// Changing state first when game is available
			changeState(StateGame);
		}
		game->focus();
		goingToMenu = false;
		fadeValue = 0;
	}
	
	bool process()
	{
		if(state == StateGame || state == StateGameEnded)
		{
			int realFrameSkip = inverseFrameSkip ? !(cycles % frameSkip) : frameSkip;
			for(int i = 0; i < realFrameSkip && (state == StateGame || state == StateGameEnded); ++i)
			{
				if(replay.get())
				{
					try
					{
						if(!replay->playbackFrame())
						{
							// End of replay
							changeState(StateGameEnded);
						}
					}
					catch(gvl::stream_error& e)
					{
						gfx.infoBox(std::string("Stream error in replay: ") + e.what());
						//Console::writeWarning(std::string("Stream error in replay: ") + e.what());
						changeState(StateGameEnded);
					}
					catch(gvl::archive_check_error& e)
					{
						gfx.infoBox(std::string("Archive error in replay: ") + e.what());
						//Console::writeWarning(std::string("Archive error in replay: ") + e.what());
						changeState(StateGameEnded);
					}
				}
				game->processFrame();
			}
			
			/*
			if(game->isGameOver()
			&& !goingToMenu)
			{
				changeState(StateGameEnded);
			}*/
		}
		
		CommonController::process();
		
		if(goingToMenu)
		{
			if(fadeValue > 0)
				fadeValue -= 1;
			else
				return false;
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
	
	void draw()
	{
		if(state == StateGame || state == StateGameEnded)
		{
			game->draw(true);
		}
		gfx.fadeValue = fadeValue;
	}
	
	void changeState(State newState)
	{
		if(state == newState)
			return;
		
		if(newState == StateGame)
			game->startGame();
		else if(newState == StateGameEnded)
		{
			if(!goingToMenu)
			{
				fadeValue = 180;
				goingToMenu = true;
			}
			replay.reset();
		}

		state = newState;
	}
		
	void swapLevel(Level& newLevel)
	{
		currentLevel()->swap(newLevel);
	}
	
	Level* currentLevel()
	{
		if(game.get() && replay.get())
			return &game->level;
		return 0;
	}

	Game* currentGame()
	{
		return game.get();
	}
	
	
	bool running()
	{
		return state != StateGameEnded && state != StateInitial;
	}
	
	std::auto_ptr<Game> game;
	State state;
	int fadeValue;
	bool goingToMenu;
	std::auto_ptr<ReplayReader> replay;
	gvl::shared_ptr<Common> common;
	//gvl::shared_ptr<Settings> settings;
};

#endif // UUID_9CD8C22BC14D4832AE2A859530FE6339
