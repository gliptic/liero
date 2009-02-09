#ifndef LIERO_SETTINGS_HPP
#define LIERO_SETTINGS_HPP

#include "worm.hpp"
#include <string>
#include <cstring>
#include <gvl/resman/shared_ptr.hpp>

struct Settings : gvl::shared
{
	enum
	{
		GMKillEmAll,
		GMGameOfTag,
		GMCtF,
		GMSimpleCtF
	};
	
	static int const selectableWeapons = 5;
	
	static int const wormAnimTab[];
	
	Settings();
	
	bool load(std::string const& path);
	void save(std::string const& path);
	
	static void generateName(WormSettings& ws);
	
	unsigned char weapTable[40];
	int maxBonuses;
	int blood;
	int timeToLose;
	int flagsToWin;
	int gameMode;
	bool shadow;
	bool loadChange;
	bool namesOnBonuses;
	bool regenerateLevel;
	int lives;
	int loadingTime;
	bool randomLevel;
	std::string levelFile;
	bool map;
	bool screenSync;
	
	gvl::shared_ptr<WormSettings> wormSettings[2];
};

#endif // LIERO_SETTINGS_HPP
