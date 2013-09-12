#ifndef LIERO_SOBJECT_HPP
#define LIERO_SOBJECT_HPP

#include "math.hpp"
#include "objectList.hpp"
#include "exactObjectList.hpp"
#include <string>

struct Worm;
struct Game;
struct WormWeapon;
struct WObject;

struct SObjectType
{
	void create(Game& game, int x, int y, int ownedIdx, WormWeapon* firedBy, WObject* from = 0);
	
	int startSound;
	int numSounds;
	int animDelay;
	int startFrame;
	int numFrames;
	int detectRange;
	int damage;
	int blowAway;
	bool shadow;
	int shake;
	int flash;
	int dirtEffect;
	
	int id;
	std::string name;
};

struct SObject : ExactObjectListBase
{
	void process(Game& game);
	
	fixed x, y;
	int id; // type
	int curFrame;
	int animDelay;
};

#endif // LIERO_SOBJECT_HPP
