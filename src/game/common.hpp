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

extern int stone_tab[3][4];

/* Textures sourced from [[constants.textures]] in tc.cfg */
/*
Textures have the reference in nObject, wObject and sObject as dirtEffect.
By changing Textures values in tc.cfg (nDrawBack, mFrame, sFrame and rFrame), you can control how
all objects (wObjects, nObjects, sObjects) and worm (via dirtEffect 0 and 7) interact with all
materials on the map (especially with dirt).
*/
struct Texture {
  bool n_draw_back;  // 1C208; causes Liero not to draw the anti-alias edges on the background.
                     // Normally turned "false" for creating dirt and rock & turned "true" for
                     // cleaning dirt.
  int m_frame;  // 1C1EA; controls which sprite is used to cut a hole (= determines the size and
                // shape of the hole).
  int s_frame;  // 1C1F4; the texture the map change will leave behind (= which sprite is used to
                // fill the hole).
  int r_frame;  // 1C1FE; the amount of sprites to use to fill the hole (starting from sFrame).
                // Note: if you set 0 or 1, then only 1 sprite will be used to fill the hole (the
                // one indicated in sFrame).
};

struct Texts {
  Texts();

  std::string game_modes[Settings::GameModes::kMaxGameModes];
  std::string onoff[NUM_ON_OFF];
  std::string controllers[3];
  std::string input_devices[3];

  static char const* key_names[177];

  std::string weap_states[NUM_WEAPON_STATES];

  int copyright_bar_format;
};

/* Colour animations sourced from [[constants.colorAnim]] in tc.cfg */
struct ColourAnim {  // sets arrays of colours which will be animated (colours will shine).
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
        original_data(std::move(other.original_data)) {
    other.sound = 0;
  }

  SfxSample& operator=(SfxSample&& other) {
    name = std::move(other.name);
    sound = other.sound;
    sound = 0;
    original_data = std::move(other.original_data);
    return *this;
  }

  SfxSample(std::string name, int length)
      : name(std::move(name)), sound(nullptr), original_data(length) {
    // A zero-length sample is a "disabled" slot. Leave `sound` null so
    // the slot survives in `Common::sounds` without occupying audio
    // memory, and so play paths can treat it as a silent no-op.
    if (length > 0) sound = SfxNewSound(length * 2);
  }

  ~SfxSample() {
    if (sound) SfxFreeSound(sound);
  }

  void CreateSound();

  std::string name;
  sfx_sound* sound;
  std::vector<uint8_t> original_data;
};

struct Bitmap;
struct FsNode;

using std::vector;

struct Common {
  Common();

  ~Common() {}

  static int fire_cone_offset[FIRE_CONE_OFFSET_DIRECTION][FIRE_CONE_OFFSET_ANGLE_FRAME]
                             [FIRE_CONE_OFFSET_XY];

  void load(FsNode node);
  void DrawTextSmall(Bitmap& scr, char const* str, int x, int y);
  void Precompute();

  std::string GuessName() const;

  // Returns the index of the named sound in `sounds`, or -1 if absent.
  // -1 is the existing "no sound" sentinel used at play sites.
  int SoundIndex(std::string_view name) const;

  PalIdx* WormSprite(int f, int dir, int w) {
    return worm_sprites.SpritePtr(f + dir * 7 * 3 + w * 2 * 7 * 3);
  }

  Sprite WormSpriteObj(int f, int dir, int w) {
    return worm_sprites[f + dir * 7 * 3 + w * 2 * 7 * 3];
  }

  PalIdx* FireConeSprite(int f, int dir) { return fire_cone_sprites.SpritePtr(f + dir * 7); }

  // Computed
  Texts texts;
  vector<int> weap_order;
  SpriteSet worm_sprites;
  SpriteSet fire_cone_sprites;

  Material materials[MAX_MATERIALS];
  Texture textures[NUM_TEXTURES];
  vector<Weapon> weapons;
  vector<SObjectType> sobject_types;
  vector<NObjectType> nobject_types;
  /* Randomized timer values for Bonus SObjects. Sourced from
   * [[constants.bonuses]] in tc.cfg (timer/timerV) */
  int bonus_rand_timer[NUM_BONUS_SOBJECTS][NUM_BONUS_TIMER_VALUES];
  /* Bonus SObjects. Sourced from [[constants.bonuses]] in tc.cfg (sobj) */
  int bonus_s_objects[NUM_BONUS_SOBJECTS];
  /* AI parameters. Sourced from [[constants.aiparams.$KEY]] in tc.cfg */
  AIParams ai_params;
  /* Colour Animations. Sourced from [[constants.colorAnim]] in tc.cfg (from/to) */
  ColourAnim color_anim[NUM_COLOR_ANIM];
  /* Bonus frames. Sourced from [[constants.bonuses]] in tc.cfg (frame) */
  int bonus_frames[NUM_BONUS_SOBJECTS];
  // all sprite sets sourced from TC/$NAME/sprites

  SpriteSet small_sprites;  // 7x7, sprites 110-239
  SpriteSet large_sprites;  // 16x16, sprites 0-109
  SpriteSet text_sprites;   // 4x4, sprites 240-265
  Palette exepal;
  Font font;
  vector<SfxSample> sounds;

  int32_t c[ConstDefT::kMaxC];
  std::string s[StringDefT::kMaxS];
  bool h[HackDefT::kMaxH];
  // Indices into `sounds` for engine-played sounds. -1 if not configured.
  int sound_hook[SoundDefT::kMaxSound] = {
#define INIT_SOUNDHOOK(n) -1,
      LIERO_SOUNDDEFS(INIT_SOUNDHOOK)
#undef INIT_SOUNDHOOK
  };
};
