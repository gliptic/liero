#ifndef LIERO_WORM_HPP
#define LIERO_WORM_HPP

#include "math.hpp"
#include <SDL/SDL.h>
#include <string>
#include <cstring>
#include <gvl/resman/shared_ptr.hpp>

#include <iostream> // TEMP

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
	
	void process(Worm& owner);
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

struct WormSettings : gvl::shared
{
	enum
	{
		Up, Down, Left, Right,
		Fire, Change, Jump,
		
		MaxControl
	};
	
	WormSettings()
	: health(100)
	, controller(0)
	, randomName(true)
	, colour(0)
	{
		rgb[0] = 26;
		rgb[1] = 26;
		rgb[2] = 62;
		
		std::memset(weapons, 0, sizeof(weapons));
	}
	
	int health;
	int controller; // CPU / Human
	Uint32 controls[MaxControl];
	int weapons[5]; // TODO: Adjustable
	std::string name;
	int rgb[3];
	bool randomName;
	
	int colour;
	int selWeapX;
};

/*
typedef struct _settings
{
 long m_iHealth[2];
 char m_iController[2];
	char m_iWeapTable[40];
 long m_iMaxBonuses;
 long m_iBlood;
 long m_iTimeToLose;
 long m_iFlagsToWin;
 char m_iGameMode;
 bool m_bShadow;
 bool m_bLoadChange;
 bool m_bNamesOnBonuses;
 bool m_bRegenerateLevel;
 BYTE m_iControls[2][7];
 BYTE m_iWeapons[2][5];
	long m_iLives;
 long m_iLoadingTime;
	bool m_bRandomLevel;
 char m_bWormName[2][21];
 char m_bLevelFile[13];
 BYTE m_iWormRGB[2][3];
 bool m_bRandomName[2];
 bool m_bMap;
 bool m_bScreenSync;
} SETTINGS, *PSETTINGS;
*/

struct Viewport;

struct Worm
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
		Left = WormSettings::Left,
		Right = WormSettings::Right,
		Up = WormSettings::Up,
		Down = WormSettings::Down,
		Fire = WormSettings::Fire,
		Change = WormSettings::Change,
		Jump = WormSettings::Jump,
		
		MaxControl
	};
	
	struct ControlState
	{
		ControlState()
		{
			std::memset(state, 0, sizeof(state));
		}
		
		bool operator==(ControlState const& b) const
		{
			for(int i = 0; i < Worm::MaxControl; ++i)
			{
				if(state[i] != b.state[i])
					return false;
			}
			
			return true;
		}
		
		bool operator!=(ControlState const& b) const
		{
			return !operator==(b);
		}
		
		bool& operator[](std::size_t n)
		{
			return state[n];
		}
		
		bool state[Worm::MaxControl];
	};
		
	Worm(gvl::shared_ptr<WormSettings> settings, int index, int wormSoundID, Game& game)
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
	, lastKilledBy(0)
	, fireCone(0)
	, leaveShellTimer(0)
	, settings(settings)
	, viewport(0)
	, index(index)
	, wormSoundID(wormSoundID)
	, direction(0)
	, game(game)
	{
		//std::memset(controlStates, 0, sizeof(controlStates));
		
		makeSightGreen = false;
		
		ready = true;
		movable = true;
		
		health = settings->health;
		visible = false;
		killedTimer = 150;
		
		//currentWeapon = 1; // This is later changed to 0, why is it here?
	}
	
	bool pressed(Control control)
	{
		return controlStates[control];
	}
	
	bool pressedOnce(Control control)
	{
		bool state = controlStates[control];
		controlStates[control] = false;
		return state;
	}
	
	void release(Control control)
	{
		controlStates[control] = false;
	}
	
	void press(Control control)
	{
		controlStates[control] = true;
	}
	
	void setControlState(Control control, bool state)
	{
		if(controlStates[control] != state)
		{
			std::cout << control << ": " << state << std::endl;
		}
		controlStates[control] = state;
	}
	
	void toggleControlState(Control control)
	{
		controlStates[control] = !controlStates[control];
	}
	
	void beginRespawn();
	void doRespawning();
	void process();
	void processWeapons();
	void processPhysics();
	void processMovement();
	void processTasks();
	void processAiming();
	void processWeaponChange();
	void processSteerables();
	void fire();
	void processSight();
	void calculateReactionForce(int newX, int newY, int dir);
	void initWeapons();
	void processLieroAI(); // Move?
	
	fixed x, y;                    //Worm position    
	fixed velX, velY;              //Worm velocity
	
	int hotspotX, hotspotY;      //Hotspots for laser, laser sight, etc.
	fixed aimingAngle, aimingSpeed;
 
	//Controls controls;
	bool ableToJump, ableToDig;   //The previous state of some keys
	bool keyChangePressed;
	bool movable;
 
	bool animate;                 //Should the worm be animated?
	bool visible;                 //Is the worm visible?
	bool ready;                   //Is the worm ready to play?
	bool flag;                    //Has the worm a flag?
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
	Worm* lastKilledBy;          // What worm that last killed this worm
	int fireCone;                //How much is left of the firecone
	int leaveShellTimer;         //Time until next shell drop
	
	gvl::shared_ptr<WormSettings> settings; // !CLONING
	Viewport* viewport; // !CLONING
	int index; // 0 or 1
	int wormSoundID;
	
	int reacts[4];
	WormWeapon weapons[5];
	int direction;
	ControlState controlStates;
	Game& game; // !CLONING
};

bool checkForWormHit(int x, int y, int dist, Worm* ownWorm);
bool checkForSpecWormHit(int x, int y, int dist, Worm& w);

#endif // LIERO_WORM_HPP
