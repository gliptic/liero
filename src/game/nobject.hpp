#ifndef LIERO_NOBJECT_HPP
#define LIERO_NOBJECT_HPP

#include "math.hpp"
#include "exactObjectList.hpp"
#include <string>

struct Worm;
struct Game;
struct WormWeapon;
struct NObject;

struct NObjectType
{
	NObject& create(Game& game, fixedvec vel, fixedvec pos, int color, int ownerIdx, WormWeapon* firedBy);
	NObject& create1(Game& game, fixedvec vel, fixedvec pos, int color, int ownerIdx, WormWeapon* firedBy);
	void create2(Game& game, int angle, fixedvec vel, fixedvec pos, int color, int ownerIdx, WormWeapon* firedBy);

	int detectDistance;
	fixed gravity;
	int speed;
	int speedV;
	int distribution;
	int blowAway;
	int bounce;
	int hitDamage;
	bool wormExplode;
	bool explGround;
	bool wormDestroy;
	int bloodOnHit;
	int startFrame;
	int numFrames;
	bool drawOnMap;
	int colorBullets;
	int createOnExp;
	bool affectByExplosions;
	int dirtEffect;
	int splinterAmount;
	int splinterColour;
	int splinterType;
	bool bloodTrail;
	int bloodTrailDelay;
	int leaveObj;
	int leaveObjDelay;
	int timeToExplo;
	int timeToExploV;

	int id;
	std::string idStr;
};

struct NObject : ExactObjectListBase
{
	void process(Game& game);

	fixedvec pos, vel;
	int timeLeft;
	//int id;
	NObjectType const* type;
	int ownerIdx;
	int curFrame;

	// STATS
	WormWeapon* firedBy;
	bool hasHit;
};

#endif // LIERO_NOBJECT_HPP
