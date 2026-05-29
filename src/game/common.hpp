#pragma once

#include <string>
#include <string_view>
#include "constants.hpp"
#include "gfx/font.hpp"
#include "gfx/palette.hpp"
#include "gfx/sprite.hpp"
#include "material.hpp"
#include "mixer/mixer.hpp"
#include "nobject.hpp"
#include "settings.hpp"
#include "sobject.hpp"
#include "weapon.hpp"

#define NUM_AIPARAMS_KEYS 7
#define NUM_AIPARAMS_VALUES 2
#define MAX_MATERIALS 256
#define NUM_TEXTURES 9
#define NUM_BONUS_SOBJECTS 2
#define NUM_BONUS_TIMER_VALUES 2
#define NUM_COLOR_ANIM 4
#define FIRE_CONE_OFFSET_DIRECTION 2
#define FIRE_CONE_OFFSET_ANGLE_FRAME 7
#define FIRE_CONE_OFFSET_XY 2
#define NUM_WEAPON_STATES 3
#define NUM_GAME_MODES 4
#define NUM_ON_OFF 2

extern int stoneTab[3][4];

/* Textures sourced from [[constants.textures]] in tc.cfg */
/* 
Textures have the reference in nObject, wObject and sObject as dirtEffect.
By changing Textures values in tc.cfg (nDrawBack, mFrame, sFrame and rFrame), you can control how all objects (wObjects, nObjects, sObjects) and worm (via dirtEffect 0 and 7) interact with all materials on the map (especially with dirt).
*/
struct Texture {
  bool nDrawBack;  // 1C208; causes Liero not to draw the anti-alias edges on the background. Normally turned "false" for creating dirt and rock & turned "true" for cleaning dirt.
  int mFrame;      // 1C1EA; controls which sprite is used to cut a hole (= determines the size and shape of the hole).
  int sFrame;      // 1C1F4; the texture the map change will leave behind (= which sprite is used to fill the hole).
  int rFrame;      // 1C1FE; the amount of sprites to use to fill the hole (starting from sFrame). Note: if you set 0 or 1, then only 1 sprite will be used to fill the hole (the one indicated in sFrame).
};

struct Texts {
  Texts();

  std::string gameModes[Settings::GameModes::MaxGameModes];
  std::string onoff[NUM_ON_OFF];
  std::string controllers[3];
  std::string inputDevices[3];

  static char const* keyNames[177];

  std::string weapStates[NUM_WEAPON_STATES];

  int copyrightBarFormat;
};

/* Colour animations sourced from [[constants.colorAnim]] in tc.cfg */
struct ColourAnim { // sets arrays of colours which will be animated (colours will shine).
  int from;
  int to;
};

/* AI parameters sourced from [[constants.aiparams]] in tc.cfg */
struct AIParams {
  int k[NUM_AIPARAMS_VALUES][NUM_AIPARAMS_KEYS];  // 0x1AEEE, contiguous words
};

struct SfxSample {
  SfxSample(SfxSample const&) = delete;
  SfxSample& operator=(SfxSample const&) = delete;

  SfxSample() : sound(0) {}

  SfxSample(SfxSample&& other)
      : name(std::move(other.name)),
        sound(other.sound),
        originalData(std::move(other.originalData)) {
    other.sound = 0;
  }

  SfxSample& operator=(SfxSample&& other) {
    name = std::move(other.name);
    sound = other.sound;
    sound = 0;
    originalData = std::move(other.originalData);
    return *this;
  }

  SfxSample(std::string name, int length)
      : name(std::move(name)), sound(nullptr), originalData(length) {
    // A zero-length sample is a "disabled" slot. Leave `sound` null so
    // the slot survives in `Common::sounds` without occupying audio
    // memory, and so play paths can treat it as a silent no-op.
    if (length > 0)
      sound = sfx_new_sound(length * 2);
  }

  ~SfxSample() {
    if (sound)
      sfx_free_sound(sound);
  }

  void createSound();

  std::string name;
  sfx_sound* sound;
  std::vector<uint8_t> originalData;
};

struct Bitmap;
struct FsNode;

using std::vector;

struct Common {
  Common();

  ~Common() {}

  static int fireConeOffset[FIRE_CONE_OFFSET_DIRECTION]
                           [FIRE_CONE_OFFSET_ANGLE_FRAME][FIRE_CONE_OFFSET_XY];

  void load(FsNode node);
  void drawTextSmall(Bitmap& scr, char const* str, int x, int y);
  void precompute();

  std::string guessName() const;

  // Returns the index of the named sound in `sounds`, or -1 if absent.
  // -1 is the existing "no sound" sentinel used at play sites.
  int soundIndex(std::string_view name) const;

  PalIdx* wormSprite(int f, int dir, int w) {
    return wormSprites.spritePtr(f + dir * 7 * 3 + w * 2 * 7 * 3);
  }

  Sprite wormSpriteObj(int f, int dir, int w) {
    return wormSprites[f + dir * 7 * 3 + w * 2 * 7 * 3];
  }

  PalIdx* fireConeSprite(int f, int dir) {
    return fireConeSprites.spritePtr(f + dir * 7);
  }

  // Computed
  Texts texts;
  vector<int> weapOrder;
  SpriteSet wormSprites;
  SpriteSet fireConeSprites;

  Material materials[MAX_MATERIALS];
  Texture textures[NUM_TEXTURES];
  vector<Weapon> weapons;
  vector<SObjectType> sobjectTypes;
  vector<NObjectType> nobjectTypes;
  /* Randomized timer values for Bonus SObjects. Sourced from
   * [[constants.bonuses]] in tc.cfg (timer/timerV) */
  int bonusRandTimer[NUM_BONUS_SOBJECTS][NUM_BONUS_TIMER_VALUES];
  /* Bonus SObjects. Sourced from [[constants.bonuses]] in tc.cfg (sobj) */
  int bonusSObjects[NUM_BONUS_SOBJECTS];
  /* AI parameters. Sourced from [[constants.aiparams.$KEY]] in tc.cfg */
  AIParams aiParams;
  /* Colour Animations. Sourced from [[constants.colorAnim]] in tc.cfg (from/to) */
  ColourAnim colorAnim[NUM_COLOR_ANIM];
  /* Bonus frames. Sourced from [[constants.bonuses]] in tc.cfg (frame) */
  int bonusFrames[NUM_BONUS_SOBJECTS];
  // all sprite sets sourced from TC/$NAME/sprites

  SpriteSet smallSprites; // 7x7, sprites 110-239
  SpriteSet largeSprites; // 16x16, sprites 0-109
  SpriteSet textSprites; // 4x4, sprites 240-265
  Palette exepal;
  Font font;
  vector<SfxSample> sounds;

  int32_t C[CONST_DEF_T::MaxC];
  std::string S[STRING_DEF_T::MaxS];
  bool H[HACK_DEF_T::MaxH];
  // Indices into `sounds` for engine-played sounds. -1 if not configured.
  int soundHook[SOUND_DEF_T::MaxSound] = {
      #define INIT_SOUNDHOOK(n) -1,
      LIERO_SOUNDDEFS(INIT_SOUNDHOOK)
      #undef INIT_SOUNDHOOK
  };
};
