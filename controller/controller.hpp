#ifndef UUID_9CD8C22BC14D4832AE2A859530FE6339
#define UUID_9CD8C22BC14D4832AE2A859530FE6339

struct Level;
struct Game;
struct Renderer;

struct Controller
{
	virtual ~Controller()
	{
	}

	// Returns true if this controller is controlling a replay, false if it is
	// an actual match
	virtual bool isReplay() { return false; };

	// Called when a key event is forwarded to the controller
	virtual void onKey(int key, bool state) = 0;

	// Called when the controller loses focus. When not focused, it will not receive key events among other things.
	virtual void unfocus() = 0;

	// Called when the controller gets focus.
	virtual void focus() = 0;

	virtual bool process() = 0;

	virtual void draw(Renderer& renderer, bool useSpectatorViewports) = 0;

	// Returns true if the game is still running. The menu should check this to decide whether to show the resume option.
	virtual bool running() = 0;

	virtual Level* currentLevel() = 0;

	virtual Game* currentGame() = 0;

	virtual void swapLevel(Level& newLevel) = 0;
};



#endif // UUID_9CD8C22BC14D4832AE2A859530FE6339
