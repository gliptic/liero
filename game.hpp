#ifndef LIERO_GAME_HPP
#define LIERO_GAME_HPP

#include <vector>
#include "level.hpp"
#include "settings.hpp"
#include "weapon.hpp"
#include "sobject.hpp"
#include "nobject.hpp"
#include "bobject.hpp"
#include "rand.hpp"
#include "bonus.hpp"
#include "sfx.hpp"
#include "constants.hpp"
#include <string>
#include <gvl/resman/shared_ptr.hpp>
#include "common.hpp"
#include "stats_recorder.hpp"

struct Viewport;
struct Worm;
struct Renderer;

struct Game
{
	Game(gvl::shared_ptr<Common> common, gvl::shared_ptr<Settings> settings);
	~Game();
	
	void onKey(Uint32 key, bool state);
	Worm* findControlForKey(uint32_t key, Worm::Control& control);
	void releaseControls();
	void processFrame();
	void gameLoop();
	void focus();
	void updateSettings();
	
	void createBObject(fixed x, fixed y, fixed velX, fixed velY);
	void createBonus();
	
	void clearViewports();
	void addViewport(Viewport*);
	void processViewports();
	void drawViewports(Renderer& renderer, bool isReplay = false);
	void clearWorms();
	void addWorm(Worm*);
	void resetWorms();
	void draw(Renderer& renderer, bool isReplay = false);
	void startGame();
	bool isGameOver();
	void createDefaults();
		
	Material pixelMat(int x, int y)
	{
		return common->materials[level.pixel(x, y)];
	}
	
	Level level;
	
	gvl::shared_ptr<Common> common;
	gvl::shared_ptr<SoundPlayer> soundPlayer;
	gvl::shared_ptr<Settings> settings;
	gvl::shared_ptr<StatsRecorder> statsRecorder;
	int screenFlash;
	bool gotChanged;
	Worm* lastKilled; // Last killed worm  !CLONING
	bool paused;
	int cycles;
	Rand rand;
	
	std::vector<Viewport*> viewports;
	std::vector<Worm*> worms;
	
	typedef ExactObjectList<Bonus, 99> BonusList;
	typedef ExactObjectList<WObject, 600> WObjectList;
	typedef ExactObjectList<SObject, 700> SObjectList;
	typedef ExactObjectList<NObject, 600> NObjectList;
	typedef FastObjectList<BObject> BObjectList;
	BonusList bonuses;
	WObjectList wobjects;
	SObjectList sobjects;
	NObjectList nobjects;
	BObjectList bobjects;
};

bool checkRespawnPosition(Game& game, int x2, int y2, int oldX, int oldY, int x, int y);

//extern Game game;

#endif // LIERO_GAME_HPP

