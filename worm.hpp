#ifndef LIERO_WORM_HPP
#define LIERO_WORM_HPP

#include "math.hpp"
#include "rand.hpp"
#include <string>
#include <cstring>
#include <memory>
#include <numeric>
#include <functional>
#include <gvl/resman/shared_ptr.hpp>
#include "version.hpp"
#include <gvl/serialization/archive.hpp> // For gvl::enable_when
#include <gvl/crypt/gash.hpp>

struct Worm;
struct Game;

struct Ninjarope
{
	Ninjarope()
	: out(false)
	, attached(false)
	, anchor(0)
	{
	}
	
	bool out;            //Is the ninjarope out?
	bool attached;
	Worm* anchor;			// If non-zero, the worm the ninjarope is attached to
	fixed x, y, velX, velY; //Ninjarope props
	// Not needed as far as I can tell: fixed forceX, forceY;
	int length, curLen;
	
	void process(Worm& owner, Game& game);
};

/*
struct Controls
{
	bool up, down, left, right;
	bool fire, change, jump;
};*/

struct WormWeapon
{
	WormWeapon()
	: id(0)
	, ammo(0)
	, delayLeft(0)
	, loadingLeft(0)
	, available(true)
	{
	}
	
	int id;
	int ammo;
	int delayLeft;
	int loadingLeft;
	bool available;			
};

struct WormSettingsExtensions
{
	enum Control
	{
		Up, Down, Left, Right,
		Fire, Change, Jump,
		Dig,
		MaxControl = Dig,
		MaxControlEx
	};
	//static const int MaxControl = Dig;

	WormSettingsExtensions()
	{
		std::memset(controlsEx, 0, sizeof(controlsEx));
	}
	
	uint32_t controlsEx[MaxControlEx];
};

struct WormSettings : gvl::shared, WormSettingsExtensions
{
	WormSettings()
	: health(100)
	, controller(0)
	, randomName(true)
	, color(0)
	{
		rgb[0] = 26;
		rgb[1] = 26;
		rgb[2] = 62;
		
        for (int i = 0; i < 5; ++i) {
            weapons[i] = 1;
        }
		std::memset(controls, 0, sizeof(controls));
	}
	
	gvl::gash::value_type& updateHash();
	
	void saveProfile(std::string const& profilePath);
	void loadProfile(std::string const& profilePath);
	
	int health;
	uint32_t controller; // CPU / Human
	uint32_t controls[MaxControl];
	uint32_t weapons[5]; // TODO: Adjustable
	std::string name;
	int rgb[3];
	bool randomName;
	
	int color;
	
	std::string profilePath;
	
	gvl::gash::value_type hash;
};

template<typename Archive>
void archive(Archive ar, WormSettings& ws)
{
	ar
	.ui32(ws.color)
	.ui32(ws.health)
	.ui16(ws.controller);
	for(int i = 0; i < WormSettings::MaxControl; ++i)
		ar.ui16(ws.controls[i]); // TODO: Initialize controlsEx from this earlier
	for(int i = 0; i < 5; ++i)
		ar.ui16(ws.weapons[i]);
	for(int i = 0; i < 3; ++i)
		ar.ui16(ws.rgb[i]);
	ar.b(ws.randomName);
	ar.str(ws.name);
	if(ar.context.replayVersion <= 1)
	{
		ws.WormSettingsExtensions::operator=(WormSettingsExtensions());
		return;
	}

	int wsVersion = myGameVersion;
	ar.ui8(wsVersion);

	for(int c = 0; c < WormSettings::MaxControl; ++c)
	{
		int dummy = 0;
		gvl::enable_when(ar, wsVersion >= 2)
			.ui8(dummy, 255)
			.ui8(dummy, 255);
	}
	
	for(int c = 0; c < WormSettings::MaxControlEx; ++c)
	{
		gvl::enable_when(ar, wsVersion >= 3)
			.ui32(ws.controlsEx[c], ws.controls[c]);
	}
}

struct Viewport;
struct Renderer;

struct WormAI : gvl::shared
{
	virtual void process(Game& game, Worm& worm) = 0;

	virtual void drawDebug(Game& game, Worm const& worm, Renderer& renderer, int offsX, int offsY)
	{
	}
};

struct DumbLieroAI : WormAI
{	
	void process(Game& game, Worm& worm);
	
	Rand rand;
};

struct Worm : gvl::shared
{
	enum
	{
		RFDown,
		RFLeft,
		RFUp,
		RFRight
	};
	
	enum Control
	{
		Up = WormSettings::Up,
		Down = WormSettings::Down,
		Left = WormSettings::Left,
		Right = WormSettings::Right,
		Fire = WormSettings::Fire,
		Change = WormSettings::Change,
		Jump = WormSettings::Jump,
		MaxControl
		//Dig = WormSettings::Dig
	};
	//static const unsigned int MaxControl = Dig;
	
	
	struct ControlState
	{
		ControlState()
		: istate(0)
		{
		}
		
		bool operator==(ControlState const& b) const
		{
			return istate == b.istate;
		}
		
		uint32_t pack() const
		{
			return istate; // & ((1 << MaxControl)-1);
		}
		
		void unpack(uint32_t state)
		{
			istate = state & 0x7f;
		}
				
		bool operator!=(ControlState const& b) const
		{
			return !operator==(b);
		}
		
		bool operator[](std::size_t n) const
		{
			return ((istate >> n) & 1) != 0;
		}
		
		void set(std::size_t n, bool v)
		{
			if(v)
				istate |= 1 << n;
			else
				istate &= ~(uint32_t(1u) << n);
		}

		void toggle(std::size_t n)
		{
			istate ^= 1 << n;
		}
		
		uint32_t istate;
	};
		
	Worm()
	: x(0), y(0), velX(0), velY(0)
	, hotspotX(0), hotspotY(0)
	, aimingAngle(0), aimingSpeed(0)
	, ableToJump(false), ableToDig(false)   //The previous state of some keys
	, keyChangePressed(false)
	, movable(false)
	, animate(false)                 //Should the worm be animated?
	, visible(false)                 //Is the worm visible?
	, ready(false)                   //Is the worm ready to play?
	, flag(false)                    //Has the worm a flag?
	, makeSightGreen(false)          //Changes the sight color
	, health(0)                  //Health left
	, lives(0)                   //lives left
	, kills(0)                   //Kills made
	, timer(0)                   //Timer for GOT
	, killedTimer(0)             //Time until worm respawns
	, currentFrame(0)
	, flags(0)                   //How many flags does this worm have?
	, currentWeapon(0)
	, fireConeActive(false)
	, lastKilledByIdx(-1)
	, fireCone(0)
	, leaveShellTimer(0)
	, index(0)
	, direction(0)
	, steerableCount(0)
	{
		makeSightGreen = false;
		
		ready = true;
		movable = true;
		
		//health = settings->health;
		visible = false;
		killedTimer = 150;
	}
	
	bool pressed(Control control) const
	{
		return controlStates[control];
	}
	
	bool pressedOnce(Control control)
	{
		bool state = controlStates[control];
		controlStates.set(control, false);
		return state;
	}
	
	void release(Control control)
	{
		controlStates.set(control, false);
	}
	
	void press(Control control)
	{
		controlStates.set(control, true);
	}
	
	void setControlState(Control control, bool state)
	{
		controlStates.set(control, state);
	}
	
	void toggleControlState(Control control)
	{
		controlStates.set(control, !controlStates[control]);
	}

	int minimapColor() const
	{
		return 129 + index * 4;
	}
		
	void beginRespawn(Game& game);
	void doRespawning(Game& game);
	void process(Game& game);
	void processWeapons(Game& game);
	void processPhysics(Game& game);
	void processMovement(Game& game);
	void processTasks(Game& game);
	void processAiming(Game& game);
	void processWeaponChange(Game& game);
	void processSteerables(Game& game);
	void fire(Game& game);
	void processSight(Game& game);
	void calculateReactionForce(Game& game, int newX, int newY, int dir);
	void initWeapons(Game& game);
	int angleFrame() const;
	
	fixed x, y;                    //Worm position    
	fixed velX, velY;              //Worm velocity

	int logicRespawnX, logicRespawnY;
	
	int hotspotX, hotspotY;      //Hotspots for laser, laser sight, etc.
	fixed aimingAngle, aimingSpeed;
 
	//Controls controls;
	bool ableToJump, ableToDig;   //The previous state of some keys
	bool keyChangePressed;
	bool movable;
 
	bool animate;                 //Should the worm be animated?
	bool visible;                 //Is the worm visible?
	bool ready;                   //Is the worm ready to play?
	bool flag;                    //Does the worm have a flag?
	bool makeSightGreen;          //Changes the sight color
	int health;                  //Health left
	int lives;                   //lives left
	int kills;                   //Kills made
	
	int timer;                   //Timer for GOT
	int killedTimer;             //Time until worm respawns
	int currentFrame;
 
	int flags;                   //How many flags does this worm have?

	Ninjarope ninjarope;
	
	int currentWeapon;           //The selected weapon
	bool fireConeActive;          //Is the firecone showing
	int lastKilledByIdx;          // What worm that last killed this worm
	int fireCone;                //How much is left of the firecone
	int leaveShellTimer;         //Time until next shell drop
	
	gvl::shared_ptr<WormSettings> settings; // !CLONING
	int index; // 0 or 1
	
	gvl::shared_ptr<WormAI> ai;
	
	int reacts[4];
	WormWeapon weapons[5];
	int direction;
	ControlState controlStates;
	ControlState prevControlStates;

	// Temporary state for steerables
	int steerableSumX, steerableSumY, steerableCount;
	
	// Data for LocalController
	ControlState cleanControlStates; // This contains the real state of real and extended controls
};

bool checkForWormHit(Game& game, int x, int y, int dist, Worm* ownWorm);
bool checkForSpecWormHit(Game& game, int x, int y, int dist, Worm& w);
int sqrVectorLength(int x, int y);


#endif // LIERO_WORM_HPP
