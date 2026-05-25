#pragma once

#include "commonController.hpp"

#include "../game.hpp"

#include "../worm.hpp"
#include "../weapsel.hpp"
#include "../replay.hpp"
#include "../console.hpp"
#include <ctime>
#include <array>

struct WeaponSelection;
struct ReplayWriter;

struct LocalController : CommonController
{
	LocalController(std::shared_ptr<Common> common, std::shared_ptr<Settings> settings);
	~LocalController();
	void onKey(int key, bool keyState);

	// Called when the controller loses focus. When not focused, it will not receive key events among other things.
	void unfocus();
	// Called when the controller gets focus.
	void focus();
	bool process();
	void draw(Renderer& renderer, bool useSpectatorViewports);
	void changeState(GameState newState);
	void endRecord();
	void swapLevel(Level& newLevel);
	Level* currentLevel();
	Game* currentGame();
	bool running();

	Game game;
	std::unique_ptr<WeaponSelection> ws;
	GameState state;
	int fadeValue;
	bool goingToMenu;
	std::unique_ptr<ReplayWriter> replay;

	// Per-worm key repeat counters for weapon selection
	static constexpr int KEY_REPEAT_INITIAL = 12;
	static constexpr int KEY_REPEAT_INTERVAL = 3;
	std::array<std::array<uint16_t, 7>, 2> wormHeldFrames{};
};
