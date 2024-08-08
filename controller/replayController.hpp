#ifndef LIERO_CONTROLLER_REPLAY_CONTROLLER_HPP
#define LIERO_CONTROLLER_REPLAY_CONTROLLER_HPP

#include "commonController.hpp"

#include "../keys.hpp"
#include "../gfx.hpp"

#include "../worm.hpp"
#include "../weapsel.hpp"
#include "../replay.hpp"
#include "../console.hpp"
#include <gvl/serialization/except.hpp>
#include <ctime>
extern "C"
{
#include "../mixer/mixer.h"
}

struct Game;

struct ReplayController : CommonController
{
	enum State
	{
		StateInitial,
		StateGame,
		StateGameEnded
	};

	ReplayController(gvl::shared_ptr<Common> common, gvl::source source);

	bool isReplay() { return true; };
	void onKey(int key, bool keyState);
	// Called when the controller loses focus. When not focused, it will not receive key events among other things.
	void unfocus();
	// Called when the controller gets focus.
	void focus();
	bool process();
	void draw(Renderer& renderer, bool useSpectatorViewports);
	void changeState(State newState);
	void swapLevel(Level& newLevel);
	Level* currentLevel();
	Game* currentGame();
	bool running();

	std::unique_ptr<Game> game;


	std::unique_ptr<Game> initialGame;
	gvl::octet_reader initialReader;


	State state;
	int fadeValue;
	bool goingToMenu;
	std::unique_ptr<ReplayReader> replay;
	gvl::shared_ptr<Common> common;

};

#endif // LIERO_CONTROLLER_REPLAY_CONTROLLER_HPP
