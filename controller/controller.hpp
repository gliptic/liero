#ifndef UUID_9CD8C22BC14D4832AE2A859530FE6339
#define UUID_9CD8C22BC14D4832AE2A859530FE6339

struct Level;
struct Game;

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

#include "../game.hpp"
#include "../worm.hpp"
#include "../weapsel.hpp"
#include "../replay.hpp"
#include "../console.hpp"
#include <gvl/serialization/except.hpp>
#include <gvl/io/stream.hpp>
#include <gvl/io/devnull.hpp>
#include <gvl/io/fstream.hpp>
#include <ctime>

#endif // UUID_9CD8C22BC14D4832AE2A859530FE6339
