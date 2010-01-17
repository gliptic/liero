#ifndef LIERO_CONTROLLER_REPLAY_CONTROLLER_HPP
#define LIERO_CONTROLLER_REPLAY_CONTROLLER_HPP

#include "commonController.hpp"

#include "../keys.hpp"
#include "../gfx.hpp"

struct ReplayController : CommonController
{
	enum State
	{
		StateInitial,
		StateGame,
		StateGameEnded
	};
	
	ReplayController(gvl::shared_ptr<Common> common, gvl::stream_ptr source);
	
	void onKey(int key, bool keyState);
	// Called when the controller loses focus. When not focused, it will not receive key events among other things.
	void unfocus();
	// Called when the controller gets focus.
	void focus();
	bool process();
	void draw();
	void changeState(State newState);
	void swapLevel(Level& newLevel);
	Level* currentLevel();
	Game* currentGame();
	bool running();
	
	std::auto_ptr<Game> game;
	State state;
	int fadeValue;
	bool goingToMenu;
	std::auto_ptr<ReplayReader> replay;
	gvl::shared_ptr<Common> common;
	//gvl::shared_ptr<Settings> settings;
};

#endif // LIERO_CONTROLLER_REPLAY_CONTROLLER_HPP
