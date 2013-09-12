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
#include <gvl/io2/stream.hpp>
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
	Texts();

	std::string gameModes[3];
	std::string onoff[2];
	std::string controllers[3];
	
	static char const* Texts::keyNames[177];

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
	Common(std::string const& lieroExe);

	~Common()
	{
		for(std::size_t i = 0; i < sounds.size(); ++i)
			sfx_free_sound(sounds[i]);
	}

	
	static int fireConeOffset[2][7][2];

	void save(std::string const& path);
	
	void loadMaterials(ReaderFile& exe);
	void loadWeapons(ReaderFile& exe);
	void loadTextures(ReaderFile& exe);
	void loadOthers(ReaderFile& exe);
	void loadConstantsFromEXE(ReaderFile& exe);
	void loadGfx(ReaderFile& exe, ReaderFile& gfx);
	void loadSfx(ReaderFile& snd);
	void loadPalette(ReaderFile& exe);
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
