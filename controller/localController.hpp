#ifndef LIERO_CONTROLLER_LOCAL_CONTROLLER_HPP
#define LIERO_CONTROLLER_LOCAL_CONTROLLER_HPP

#include "commonController.hpp"

#include "../game.hpp"

#include "../worm.hpp"
#include "../weapsel.hpp"
#include "../replay.hpp"
#include "../console.hpp"
#include <gvl/serialization/except.hpp>
#include <gvl/io/stream.hpp>
#include <gvl/io/fstream.hpp>
#include <ctime>

struct WeaponSelection;
struct ReplayWriter;

struct LocalController : CommonController
{
	enum State
	{
		StateInitial,
		StateWeaponSelection,
		StateGame,
		StateGameEnded
	};
	
	LocalController(gvl::shared_ptr<Common> common, gvl::shared_ptr<Settings> settings);
	~LocalController();
	void onKey(int key, bool keyState);
	
	// Called when the controller loses focus. When not focused, it will not receive key events among other things.
	void unfocus();
	// Called when the controller gets focus.
	void focus();
	bool process();
	void draw();
	void changeState(State newState);
	void endRecord();
	void swapLevel(Level& newLevel);
	Level* currentLevel();
	Game* currentGame();
	bool running();
	
	Game game;
	std::auto_ptr<WeaponSelection> ws;
	State state;
	int fadeValue;
	bool goingToMenu;
	std::auto_ptr<ReplayWriter> replay;
};

#endif // LIERO_CONTROLLER_LOCAL_CONTROLLER_HPP
