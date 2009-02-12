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
	
	virtual Level& currentLevel() = 0;
	
	virtual void swapLevel(Level& newLevel) = 0;
};

#include "game.hpp"
#include "worm.hpp"
#include "weapsel.hpp"
#include "replay.hpp"
#include <gvl/io/stream.hpp>
#include <gvl/io/devnull.hpp>
#include <gvl/io/fstream.hpp>

struct LocalController : Controller
{
	enum State
	{
		StateWeaponSelection,
		StateGame,
		StateGameEnded
	};
	
	LocalController(gvl::shared_ptr<Common> common, gvl::shared_ptr<Settings> settings)
	: game(common, settings)
	, state(StateGameEnded)
	, fadeValue(0)
	, shutDown(false)
	{
		game.createDefaults();
	}
	
	void onKey(int key, bool keyState)
	{
		Worm::Control control;
		Worm* worm = game.findControlForKey(key, control);
		if(worm)
		{
			worm->setControlState(control, keyState);
			/*
			if(state == StateGame)
				getReplay().setControlState(worm, control, keyState);*/
		}
				
		if(key == DkEscape && !shutDown)
		{
			fadeValue = 31;
			shutDown = true;
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
			changeState(StateWeaponSelection);
		game.enter();
		shutDown = false;
		fadeValue = 0;
	}
	
	bool process()
	{
		if(state == StateWeaponSelection)
		{
			if(ws->processFrame()) // TODO: Go back to menu if escape is pressed in the weapon selection
				changeState(StateGame);
		}
		else if(state == StateGame)
		{
			
			getReplay().recordFrame(game);
			game.processFrame();
			
			if(game.isGameOver()
			&& !shutDown)
			{
				fadeValue = 180;
				shutDown = true;
			}
		}
		
		if(shutDown)
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
		if(state == StateWeaponSelection)
		{
			ws->draw();
		}
		else if(state == StateGame || state == StateGameEnded)
		{
			game.draw();
		}
		gfx.fadeValue = fadeValue;
	}
	
	void changeState(State newState)
	{
		if(state == newState)
			return;
			
		if(state == StateWeaponSelection)
		{
			ws->finalize();
			game.soundPlayer->play(22, 22);
			fadeValue = 33;
			ws.reset();
		}
		
		if(newState == StateWeaponSelection)
		{
			ws.reset(new WeaponSelection(game));
		}
		
		state = newState;
	}
	
	Replay& getReplay()
	{
		if(!replay.get())
		{
			replay.reset(new Replay(gvl::stream_ptr(new gvl::fstream(std::fopen("test.lrp", "wb")))));
			replay->beginRecord(game);
		}
		return *replay;
	}
	
	void swapLevel(Level& newLevel)
	{
		currentLevel().swap(newLevel);
	}
	
	Level& currentLevel()
	{
		return game.level;
	}
		
	bool running()
	{
		return state != StateGameEnded;
	}
	
	Game game;
	std::auto_ptr<WeaponSelection> ws;
	State state;
	int fadeValue;
	bool shutDown;
	std::auto_ptr<Replay> replay;
};

struct ReplayController : Controller
{
	enum State
	{
		StateGame,
		StateGameEnded
	};
	
	ReplayController(gvl::shared_ptr<Common> common, gvl::shared_ptr<Settings> settings)
	: state(StateGameEnded)
	, fadeValue(0)
	, shutDown(false)
	, replay(gvl::stream_ptr(new gvl::fstream(std::fopen("test.lrp", "rb"))))
	, common(common)
	, settings(settings)
	{
	}
	
	void onKey(int key, bool keyState)
	{
		if(key == DkEscape && !shutDown)
		{
			fadeValue = 31;
			shutDown = true;
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
			//game.reset(new Game(common, settings));
			//replay.beginPlayback(*game);
			game = replay.beginPlayback(common);
			changeState(StateGame);
		}
		game->enter();
		shutDown = false;
		fadeValue = 0;
	}
	
	bool process()
	{
		if(state == StateGame)
		{
			try
			{
				getReplay().playbackFrame(*game);
			}
			catch(gvl::stream_error& e)
			{
				shutDown = true;
			}
			game->processFrame();
			
			if(game->isGameOver()
			&& !shutDown)
			{
				fadeValue = 180;
				shutDown = true;
			}
		}
		
		if(shutDown)
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
			game->draw();
		}
		gfx.fadeValue = fadeValue;
	}
	
	void changeState(State newState)
	{
		if(state == newState)
			return;

		state = newState;
	}
		
	void swapLevel(Level& newLevel)
	{
		currentLevel().swap(newLevel);
	}
	
	Level& currentLevel()
	{
		return game->level;
	}
	
	Replay& getReplay()
	{
		return replay;
	}
		
	bool running()
	{
		return state != StateGameEnded;
	}
	
	std::auto_ptr<Game> game;
	State state;
	int fadeValue;
	bool shutDown;
	Replay replay;
	gvl::shared_ptr<Common> common;
	gvl::shared_ptr<Settings> settings;
};

#endif // UUID_9CD8C22BC14D4832AE2A859530FE6339
