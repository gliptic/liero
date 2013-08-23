#ifndef UUID_9E238CFB9F074A3A432E22AE5B8EE5FB
#define UUID_9E238CFB9F074A3A432E22AE5B8EE5FB

#include "gfx/font.hpp"
#include "weapon.hpp"
#include "sobject.hpp"
#include "nobject.hpp"
#include "constants.hpp"
#include "material.hpp"
#include "gfx/palette.hpp"
#include "gfx/sprite.hpp"
#include <string>
#include <gvl/resman/shared_ptr.hpp>
extern "C" {
#include "mixer/mixer.h"
}

extern int stoneTab[3][4];

struct Texture
{
	bool nDrawBack; // 1C208
	int  mFrame; // 1C1EA
	int  sFrame; // 1C1F4
	int  rFrame; // 1C1FE
};

struct Texts
{
	void loadFromEXE();

	//std::string copyright1;
	std::string copyright2;
	std::string loadoptions;
	std::string saveoptions;
	std::string curOptNoFile;
	std::string curOpt;
	
	std::string gameModes[3];
	std::string gameModeSpec[3];
	std::string onoff[2];
	std::string controllers[3];
	
	std::string keyNames[177];
	
	std::string random;
	std::string random2;
	std::string reloadLevel;
	std::string regenLevel;
	std::string selWeap;
	std::string levelRandom;
	std::string levelIs1;
	std::string levelIs2;
	std::string randomize;
	std::string done;
	
	std::string kills;
	std::string lives;
	
	std::string suicide;
	std::string reloading;
	std::string pressFire;
	std::string selLevel;
	
	std::string noWeaps;
	std::string weapon;
	std::string availability;
	std::string weapStates[3];
	
	
	int copyrightBarFormat;
	
};

struct ColourAnim
{
	int from;
	int to;
};

struct AIParams
{
	int k[2][7]; // 0x1AEEE, contiguous words
};

struct Bitmap;

struct Common : gvl::shared
{
	Common()
	{
	}

	~Common()
	{
		for(std::size_t i = 0; i < sounds.size(); ++i)
			sfx_free_sound(sounds[i]);
	}

	
	static int fireConeOffset[2][7][2];
	
	void loadMaterials();
	void loadWeapons();
	void loadTextures();
	void loadOthers();
	void loadConstantsFromEXE();
	void loadGfx();
	void loadSfx();
	void loadPalette();
	void drawTextSmall(Bitmap& scr, char const* str, int x, int y);
	
	PalIdx* wormSprite(int f, int dir, int w)
	{
		return wormSprites.spritePtr(f + dir*7*3 + w*2*7*3);
	}

	Sprite wormSpriteObj(int f, int dir, int w)
	{
		return wormSprites[f + dir*7*3 + w*2*7*3];
	}
	
	PalIdx* fireConeSprite(int f, int dir)
	{
		return fireConeSprites.spritePtr(f + dir*7);
	}
	
	Material materials[256];
	Texts texts;
	Texture textures[9];
	Weapon weapons[40];
	SObjectType sobjectTypes[14];
	NObjectType nobjectTypes[24];
	int weapOrder[41]; // 1-based!
	int bonusRandTimer[2][2];
	int bonusSObjects[2];
	AIParams aiParams;
	ColourAnim colorAnim[4];
	int bonusFrames[2];
	SpriteSet smallSprites;
	SpriteSet largeSprites;
	SpriteSet textSprites;
	SpriteSet wormSprites;
	SpriteSet fireConeSprites;
	Palette exepal;
	Font font;
	std::vector<sfx_sound*> sounds;
	
	int C[MaxC];
	std::string S[MaxS];
	bool H[MaxH];
};

#endif // UUID_9E238CFB9F074A3A432E22AE5B8EE5FB
