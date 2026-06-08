#include "common_exereader.hpp"

#include <cctype>
#include "game/cp437.hpp"
#include "game/filesystem.hpp"
#include "game/io/coding.hpp"
#include "game/rand.hpp"
#include "game/reader.hpp"

static int c_sint32desc[][3] = {{CNRInitialLength, 0x32D7, 0x32DD},
                                {CNRAttachLength, 0xA679, 0xA67F},

                                {0, -1, -1}};

static int c_sint24desc[][3] = {{CMinBounceUp, 0x3B7D, 0x3B74},
                                {CMinBounceDown, 0x3B00, 0x3AF7},
                                {CMinBounceLeft, 0x3A83, 0x3A7A},
                                {CMinBounceRight, 0x3A06, 0x39FD},
                                {CWormGravity, 0x3BDE, 0x3BD7},
                                {CWalkVelLeft, 0x3F97, 0x3F9D},
                                {CMaxVelLeft, 0x3F8C, 0x3F83},
                                {CWalkVelRight, 0x4018, 0x401E},
                                {CMaxVelRight, 0x400D, 0x4004},
                                {CJumpForce, 0x3327, 0x332D},
                                {CMaxAimVelLeft, 0x30F2, 0x30E9},
                                {CAimAccLeft, 0x30FD, 0x3103},
                                {CMaxAimVelRight, 0x311A, 0x3111},
                                {CAimAccRight, 0x3125, 0x312B},
                                {CNinjaropeGravity, 0xA895, 0xA89B},
                                {CNRMinLength, 0x3206, 0x31FD},
                                {CNRMaxLength, 0x3229, 0x3220},

                                {CBonusGravity, 0x72C3, 0x72C9},
                                {CBObjGravity, 0x744A, 0x7450},

                                // WormFloat hack
                                {CWormFloatPower, 0x29DB, 0x29E1},

                                {0, -1, -1}};

static int c_uint16desc[][2] = {{CBloodLimit, 0xE686},

                                {0, -1}};

static int c_sint16desc[][2] = {
    {CWormFricMult, 0x39BD},
    {CWormFricDiv, 0x39C7},
    {CWormMinSpawnDistLast, 0x242E},
    {CWormMinSpawnDistEnemy, 0x244B},
    {CWormSpawnRectX, 0x4913},
    {CWormSpawnRectY, 0x4925},
    {CWormSpawnRectW, 0x490B},
    {CWormSpawnRectH, 0x491D},
    {CAimFricMult, 0x3003},
    {CAimFricDiv, 0x300D},

    {CNRThrowVelX, 0x329B},
    {CNRThrowVelY, 0x32BF},
    {CNRForceShlX, 0xA8AD},
    {CNRForceDivX, 0xA8B7},
    {CNRForceShlY, 0xA8DA},
    {CNRForceDivY, 0xA8E4},
    {CNRForceLenShl, 0xA91E},

    {CBonusBounceMul, 0x731F},
    {CBonusBounceDiv, 0x7329},
    {CBonusFlickerTime, 0x87B8},

    {CBonusDropChance, 0xBECA},
    {CSplinterLarpaVelDiv, 0x677D},
    {CSplinterCracklerVelDiv, 0x67D0},

    // WormFloat hack
    {CWormFloatLevel, 0x29D3},

    // BonusSpawnRect hack
    {CBonusSpawnRectX, 0x2319},
    {CBonusSpawnRectY, 0x2327},
    {CBonusSpawnRectW, 0x2311},  // This is used even when the hack isn't enabled
    {CBonusSpawnRectH, 0x231F},  // -==-

    {0, -1}};

static int c_uint8desc[][2] = {{CAimMaxRight, 0x3030},
                               {CAimMinRight, 0x304A},
                               {CAimMaxLeft, 0x3066},
                               {CAimMinLeft, 0x3080},
                               {CNRColourBegin, 0x10FD2},
                               {CNRColourEnd, 0x11069},
                               {CBonusExplodeRisk, 0x2DB2},
                               {CBonusHealthVar, 0x2D56},
                               {CBonusMinHealth, 0x2D5D},
                               {CLaserWeapon, 0x7255},

                               {CFirstBloodColour, 0x2388},
                               {CNumBloodColours, 0x2381},

                               {CRemExpObject, 0x8F8B},

                               {0, -1}};

static int c_sint8desc[][2] = {{CNRPullVel, 0x31D0},
                               {CNRReleaseVel, 0x31F0},

                               // FallDamage hack
                               {CFallDamageRight, 0x3A0E},
                               {CFallDamageLeft, 0x3A8B},
                               {CFallDamageDown, 0x3B08},
                               {CFallDamageUp, 0x3B85},

                               {CBloodStepUp, 0xE67B},
                               {CBloodStepDown, 0xE68E},

                               {0, -1}};

static int sstringdesc[][2] = {{SInitSound, 0x177F},
                               {SLoadingSounds, 0x18F2},

                               {SInit_BaseIO, 0x17DD},
                               {SInit_IRQ, 0x17E5},
                               {SInit_DMA8, 0x17EE},
                               {SInit_DMA16, 0x17F8},

                               {SInit_DSPVersion, 0x181E},
                               {SInit_Colon, 0x182B},
                               {SInit_16bit, 0x182F},
                               {SInit_Autoinit, 0x1840},

                               {SInit_XMSSucc, 0x189D},

                               {SInit_FreeXMS, 0x18C5},
                               {SInit_k, 0x18D8},

                               {SLoadingAndThinking, 0xFB92},
                               {SOK, 0xFBA8},
                               {SOK2, 0x190E},
                               {SPressAnyKey, 0xFBAB},

                               {SCommittedSuicideMsg, 0xE70C},
                               {SKilledMsg, 0xE71F},
                               {SYoureIt, 0x75C5},

                               // Pascal strings
                               {SRandom, 0xD6E3},
                               {SRandom2, 0xD413},
                               {SRegenLevel, 0xD41A},
                               {SReloadLevel, 0xD42D},
                               {SCopyright, 0xFB60},
                               {SCopyright2, 0xE693},
                               {SSelWeap, 0xA9C0},
                               {SLevelRandom, 0xA9D5},
                               {SLevelIs1, 0xA9E3},
                               {SLevelIs2, 0xA9EC},
                               {SRandomize, 0xA9F4},
                               {SDone, 0xA9EE},
                               {SReloading, 0x7583},
                               {SPressFire, 0x7590},
                               {SKills, 0x75A4},
                               {SLives, 0x75AC},
                               {SSelLevel, 0xD6F2},
                               {SWeapon, 0xD700},
                               {SAvailability, 0xD707},
                               {SNoWeaps, 0xD714},

                               {0, -1}};

struct HackDesc {
  int which;
  int (*indicators)[2];
};

static int h_fall_damage_ind[][2] = {{0x3A0A, 0x26},
                                     {0x3A87, 0x26},
                                     {0x3B04, 0x26},
                                     {0x3B81, 0x26},

                                     {-1, 0}};

static int h_bonus_reload_only_ind[][2] = {
    {0x2DB1, 0xEB},  // We check one byte only, because ProMode has a silly jump destination

    {-1, 0}};

static int h_bonus_spawn_rect_ind[][2] = {
    {0x2318, 0x05},  // These are the first bytes of the add instructions that offset the spawn
    {0x2323, 0x05},

    {-1, 0}};

static int h_bonus_only_health_ind[][2] = {{0x228B, 0xB0},
                                           {0x228C, 0x02},

                                           {-1, 0}};

static int h_bonus_only_weapon_ind[][2] = {{0x228B, 0xB0},
                                           {0x228C, 0x01},

                                           {-1, 0}};

static int h_bonus_disable_ind[][2] = {{0xBED3, 0xEB},

                                       {-1, 0}};

static int h_worm_float_ind[][2] = {
    {0x29D7, 0x26},  // 0x26 is the first byte of the sub instruction
    {0x29DA, 0x34},  // 0x34 is the offset to part of velY of the worm

    {-1, 0}};

static int h_rem_exp_ind[][2] = {
    // Start of the mov instruction that zeroes the timeout counter
    {0x8fc9, 0x26},
    {0x8fca, 0xc7},
    {-1, 0}};

static int h_signed_recoil_ind[][2] = {{0x38AC, 0x98}, {0x38EC, 0x98}, {-1, 0}};

static int h_air_jump_ind[][2] = {{0x3313, 0xEB}, {0x3314, 0x06}, {-1, 0}};

static int h_multi_jump_ind[][2] = {{0x331B, 0xEB}, {0x331C, 0x06}, {-1, 0}};

static HackDesc hhackdesc[] = {{.which = HFallDamage, .indicators = h_fall_damage_ind},
                               {.which = HBonusReloadOnly, .indicators = h_bonus_reload_only_ind},
                               {.which = HBonusSpawnRect, .indicators = h_bonus_spawn_rect_ind},
                               {.which = HWormFloat, .indicators = h_worm_float_ind},
                               {.which = HBonusOnlyHealth, .indicators = h_bonus_only_health_ind},
                               {.which = HBonusOnlyWeapon, .indicators = h_bonus_only_weapon_ind},
                               {.which = HBonusDisable, .indicators = h_bonus_disable_ind},
                               {.which = HRemExp, .indicators = h_rem_exp_ind},
                               {.which = HSignedRecoil, .indicators = h_signed_recoil_ind},
                               {.which = HAirJump, .indicators = h_air_jump_ind},
                               {.which = HMultiJump, .indicators = h_multi_jump_ind},
                               {.which = 0, .indicators = nullptr}};

static char const* sobject_names[14] = {"Large explosion",
                                        "Medium explosion",
                                        "Small explosion",
                                        "Hellraider smoke",
                                        "Zimm flash",
                                        "Nuke smoke",
                                        "Flashing pixel",
                                        "Teleport flash",
                                        "Small explosion, silent",
                                        "Very small explosion, silent",
                                        "Medium explosion, smaller",
                                        "Large explosion, smaller",
                                        "Medium explosion, bigger",
                                        "Unknown"};

static char const* nobject_names[24] = {"Worm 1 parts",
                                        "Worm 2 parts",
                                        "Particle, disappearing",
                                        "Particle, small damage",
                                        "Particle, medium damage",
                                        "Particle, larger damage",
                                        "Blood",
                                        "Shells",
                                        "Clusterbomb bombs",
                                        "Large nukes",
                                        "Hellraider bullets",
                                        "Small nukes",
                                        "Napalm fireballs",
                                        "Dirt",
                                        "Chiquitabomb bombs",
                                        "Grasshopper 1",
                                        "Grasshopper 2",
                                        "Grasshopper 3",
                                        "Grasshopper 4",
                                        "Grasshopper 5",
                                        "Flag 1",
                                        "Flag 2",
                                        "Grasshopper 6",
                                        "Grasshopper 7"};

static std::string ToId(std::string const& name) {
  std::string ret;
  for (char const kC : name) {
    if (static_cast<uint8_t>(kC) >= 128 || !std::isalnum(static_cast<uint8_t>(kC)))
      ret += '_';
    else
      ret += std::tolower(static_cast<uint8_t>(kC));
  }
  return ret;
}

static inline std::string ReadPascalString(ReaderFile& f) {
  unsigned char const kLength = f.Get();

  char txt[256];
  f.Get(reinterpret_cast<uint8_t*>(txt), kLength);
  // NOLINTNEXTLINE(modernize-return-braced-init-list) — braced init would pick the initializer-list ctor, not the (ptr, count) ctor.
  return std::string(txt, kLength);
}

static inline std::string ReadPascalString(ReaderFile& f, unsigned char field_len) {
  char txt[256];
  f.Get(reinterpret_cast<uint8_t*>(txt), field_len);

  auto const kLength = static_cast<unsigned char>(txt[0]);
  // NOLINTNEXTLINE(modernize-return-braced-init-list) — braced init would pick the initializer-list ctor, not the (ptr, count) ctor.
  return std::string(txt + 1, kLength);
}

static inline std::string ReadPascalStringAt(ReaderFile& f, size_t location) {
  f.Seekg(location);
  return ReadPascalString(f);
}

static void LoadConstants(Common& common, ReaderFile& exe) {
  for (int i = 0; c_sint32desc[i][1] >= 0; ++i) {
    exe.Seekg(c_sint32desc[i][1]);
    auto const kA = static_cast<int32_t>(io::ReadUint16Le(exe));
    exe.Seekg(c_sint32desc[i][2]);
    int32_t const kB = static_cast<int16_t>(io::ReadUint16Le(exe));
    common.c[c_sint32desc[i][0]] = kA + (kB << 16);
  }

  for (int i = 0; c_sint24desc[i][1] >= 0; ++i) {
    exe.Seekg(c_sint24desc[i][1]);
    auto const kA = static_cast<int32_t>(io::ReadUint16Le(exe));
    exe.Seekg(c_sint24desc[i][2]);
    // Intentional signed widening: the .exe file stores a sign-extended 24-bit
    // value as low-16 unsigned + high-8 signed.
    // NOLINTNEXTLINE(bugprone-signed-char-misuse, cert-str34-c)
    int32_t const kB = static_cast<int8_t>(exe.Get());
    common.c[c_sint24desc[i][0]] = kA + (kB << 16);
  }

  for (int i = 0; c_sint16desc[i][1] >= 0; ++i) {
    exe.Seekg(c_sint16desc[i][1]);
    common.c[c_sint16desc[i][0]] = static_cast<int16_t>(io::ReadUint16Le(exe));
  }

  for (int i = 0; c_uint16desc[i][1] >= 0; ++i) {
    exe.Seekg(c_uint16desc[i][1]);
    common.c[c_uint16desc[i][0]] = io::ReadUint16Le(exe);
  }

  for (int i = 0; c_sint8desc[i][1] >= 0; ++i) {
    exe.Seekg(c_sint8desc[i][1]);
    // NOLINTNEXTLINE(bugprone-signed-char-misuse, cert-str34-c) — c_sint8desc fields are signed 8-bit by definition.
    common.c[c_sint8desc[i][0]] = static_cast<int8_t>(exe.Get());
  }

  for (int i = 0; c_uint8desc[i][1] >= 0; ++i) {
    exe.Seekg(c_uint8desc[i][1]);
    common.c[c_uint8desc[i][0]] = exe.Get();
  }

  for (int i = 0; sstringdesc[i][1] >= 0; ++i) {
    // Strings in the EXE are CP437 (DOS Liero); tc.cfg stores UTF-8.
    common.s[sstringdesc[i][0]] =
        cp437::Cp437BytesToUtf8(ReadPascalStringAt(exe, sstringdesc[i][1]));
  }

  for (int i = 0; hhackdesc[i].indicators; ++i) {
    int const(*ind)[2] = hhackdesc[i].indicators;
    bool active = true;
    for (; (*ind)[0] >= 0; ++ind) {
      exe.Seekg((*ind)[0]);
      int const kB = exe.Get();
      if (kB != (*ind)[1]) {
        active = false;
        break;
      }
    }

    common.h[hhackdesc[i].which] = active;
  }
}

static void LoadPalette(Common& common, ReaderFile& exe) {
  exe.Seekg(132774);

  for (auto& entrie : common.exepal.entries) {
    unsigned char rgb[3];
    exe.Get(reinterpret_cast<uint8_t*>(rgb), 3);

    entrie.r = rgb[0] & 63;
    entrie.g = rgb[1] & 63;
    entrie.b = rgb[2] & 63;
  }

  exe.Seekg(0x1AF0C);
  for (auto& i : common.color_anim) {
    i.from = exe.Get();
    i.to = exe.Get();
  }
}

static void LoadMaterials(Common& common, ReaderFile& exe) {
  exe.Seekg(0x01C2E0);

  for (auto& material : common.materials) {
    material.flags = 0;
  }

  unsigned char bits[32];

  for (int i = 0; i < 5; ++i) {
    exe.Get(reinterpret_cast<uint8_t*>(bits), 32);

    for (int j = 0; j < 256; ++j) {
      int const kBit = ((bits[j >> 3] >> (j & 7)) & 1);
      common.materials[j].flags |= kBit << i;
    }
  }

  exe.Seekg(0x01AEA8);

  exe.Get(reinterpret_cast<uint8_t*>(bits), 32);

  for (int j = 0; j < 256; ++j) {
    int const kBit = ((bits[j >> 3] >> (j & 7)) & 1);
    common.materials[j].flags |= kBit << 5;
  }
}

struct Read32 {
  static int32_t Run(ReaderFile& f) { return static_cast<int32_t>(io::ReadUint32Le(f)); }
};

struct Read16 {
  static int32_t Run(ReaderFile& f) {
    return static_cast<int32_t>(static_cast<int16_t>(io::ReadUint16Le(f)));
  }
};

struct Read8 {
  static int32_t Run(ReaderFile& f) { return f.Get(); }
};

struct ReadBool {
  static bool Run(ReaderFile& f) { return f.Get() != 0; }
};

template <typename T>
struct Dec {
  static int32_t Run(ReaderFile& f) { return T::Run(f) - 1; }
};

template <typename Reader, typename T, int N, typename U>
static inline void ReadMembers(ReaderFile& f, T (&arr)[N], U(T::* mem)) {
  for (int i = 0; i < N; ++i) {
    (arr[i].*mem) = Reader::Run(f);
  }
}

template <typename Reader, typename T, typename U>
static inline void ReadMembers(ReaderFile& f, std::vector<T>& arr, U(T::* mem)) {
  for (int i = 0; i < arr.size(); ++i) {
    (arr[i].*mem) = Reader::Run(f);
  }
}

static void LoadWeapons(Common& common, ReaderFile& exe) {
  exe.Seekg(112806);

  ReadMembers<Read8>(exe, common.weapons, &Weapon::detect_distance);
  ReadMembers<ReadBool>(exe, common.weapons, &Weapon::affect_by_worm);
  ReadMembers<Read8>(exe, common.weapons, &Weapon::blow_away);

  exe.Seekg(112966);
  ReadMembers<Read16>(exe, common.weapons, &Weapon::gravity);
  ReadMembers<ReadBool>(exe, common.weapons, &Weapon::shadow);
  ReadMembers<ReadBool>(exe, common.weapons, &Weapon::laser_sight);
  ReadMembers<Dec<Read8> >(exe, common.weapons, &Weapon::launch_sound);
  ReadMembers<ReadBool>(exe, common.weapons, &Weapon::loop_sound);
  ReadMembers<Dec<Read8> >(exe, common.weapons, &Weapon::explo_sound);
  ReadMembers<Read16>(exe, common.weapons, &Weapon::speed);
  ReadMembers<Read16>(exe, common.weapons, &Weapon::add_speed);
  ReadMembers<Read16>(exe, common.weapons, &Weapon::distribution);
  ReadMembers<Read8>(exe, common.weapons, &Weapon::parts);
  ReadMembers<Read8>(exe, common.weapons, &Weapon::recoil);
  ReadMembers<Read16>(exe, common.weapons, &Weapon::mult_speed);
  ReadMembers<Read16>(exe, common.weapons, &Weapon::delay);
  ReadMembers<Read16>(exe, common.weapons, &Weapon::loading_time);
  ReadMembers<Read8>(exe, common.weapons, &Weapon::ammo);
  ReadMembers<Dec<Read8> >(exe, common.weapons, &Weapon::create_on_exp);
  ReadMembers<Dec<Read8> >(exe, common.weapons, &Weapon::dirt_effect);
  ReadMembers<Read8>(exe, common.weapons, &Weapon::leave_shells);
  ReadMembers<Read8>(exe, common.weapons, &Weapon::leave_shell_delay);
  ReadMembers<ReadBool>(exe, common.weapons, &Weapon::play_reload_sound);
  ReadMembers<ReadBool>(exe, common.weapons, &Weapon::worm_explode);
  ReadMembers<ReadBool>(exe, common.weapons, &Weapon::expl_ground);
  ReadMembers<ReadBool>(exe, common.weapons, &Weapon::worm_collide);
  ReadMembers<Read8>(exe, common.weapons, &Weapon::fire_cone);
  ReadMembers<ReadBool>(exe, common.weapons, &Weapon::collide_with_objects);
  ReadMembers<ReadBool>(exe, common.weapons, &Weapon::affect_by_explosions);
  ReadMembers<Read8>(exe, common.weapons, &Weapon::bounce);
  ReadMembers<Read16>(exe, common.weapons, &Weapon::time_to_explo);
  ReadMembers<Read16>(exe, common.weapons, &Weapon::time_to_explo_v);
  ReadMembers<Read8>(exe, common.weapons, &Weapon::hit_damage);
  ReadMembers<Read8>(exe, common.weapons, &Weapon::blood_on_hit);
  ReadMembers<Read16>(exe, common.weapons, &Weapon::start_frame);
  ReadMembers<Read8>(exe, common.weapons, &Weapon::num_frames);
  ReadMembers<ReadBool>(exe, common.weapons, &Weapon::loop_anim);
  ReadMembers<Read8>(exe, common.weapons, &Weapon::shot_type);
  ReadMembers<Read8>(exe, common.weapons, &Weapon::color_bullets);
  ReadMembers<Read8>(exe, common.weapons, &Weapon::splinter_amount);
  ReadMembers<Read8>(exe, common.weapons, &Weapon::splinter_colour);
  ReadMembers<Dec<Read8> >(exe, common.weapons, &Weapon::splinter_type);
  ReadMembers<Read8>(exe, common.weapons, &Weapon::splinter_scatter);
  ReadMembers<Dec<Read8> >(exe, common.weapons, &Weapon::obj_trail_type);
  ReadMembers<Read8>(exe, common.weapons, &Weapon::obj_trail_delay);
  ReadMembers<Read8>(exe, common.weapons, &Weapon::part_trail_type);
  ReadMembers<Dec<Read8> >(exe, common.weapons, &Weapon::part_trail_obj);
  ReadMembers<Read8>(exe, common.weapons, &Weapon::part_trail_delay);

  exe.Seekg(0x1B676);
  for (int i = 0; i < 40; ++i) {
    // Read CP437 bytes from the EXE; derive idStr from those (toId() lowercases
    // ASCII and replaces everything else with '_', so single-byte input keeps
    // the id length predictable), then transcode the displayable name to UTF-8.
    std::string const kRaw = ReadPascalString(exe, 14);
    common.weapons[i].id_str = ToId(kRaw);
    common.weapons[i].name = cp437::Cp437BytesToUtf8(kRaw);
    common.weapons[i].id = i;
    common.weapons[i].chain_explosion = i == 34;
  }

  // Special objects
  exe.Seekg(115218);
  ReadMembers<Dec<Read8> >(exe, common.sobject_types, &SObjectType::start_sound);
  ReadMembers<Read8>(exe, common.sobject_types, &SObjectType::num_sounds);
  ReadMembers<Read8>(exe, common.sobject_types, &SObjectType::anim_delay);
  ReadMembers<Read8>(exe, common.sobject_types, &SObjectType::start_frame);
  ReadMembers<Read8>(exe, common.sobject_types, &SObjectType::num_frames);
  ReadMembers<Read8>(exe, common.sobject_types, &SObjectType::detect_range);
  ReadMembers<Read8>(exe, common.sobject_types, &SObjectType::damage);
  ReadMembers<Read32>(exe, common.sobject_types,
                      &SObjectType::blow_away);  // blowAway has 13 slots, not 14. The last value
                                                 // will overlap with shadow.

  exe.Seekg(115368);
  ReadMembers<ReadBool>(exe, common.sobject_types, &SObjectType::shadow);
  ReadMembers<Read8>(exe, common.sobject_types, &SObjectType::shake);
  ReadMembers<Read8>(exe, common.sobject_types, &SObjectType::flash);
  ReadMembers<Dec<Read8> >(exe, common.sobject_types, &SObjectType::dirt_effect);

  for (int i = 0; i < 14; ++i)  // TODO: Unhardcode
  {
    common.sobject_types[i].id = i;
  }

  exe.Seekg(111430);

  ReadMembers<Read8>(exe, common.nobject_types, &NObjectType::detect_distance);
  ReadMembers<Read16>(exe, common.nobject_types, &NObjectType::gravity);
  ReadMembers<Read16>(exe, common.nobject_types, &NObjectType::speed);
  ReadMembers<Read16>(exe, common.nobject_types, &NObjectType::speed_v);
  ReadMembers<Read16>(exe, common.nobject_types, &NObjectType::distribution);
  ReadMembers<Read8>(exe, common.nobject_types, &NObjectType::blow_away);
  ReadMembers<Read8>(exe, common.nobject_types, &NObjectType::bounce);
  ReadMembers<Read8>(exe, common.nobject_types, &NObjectType::hit_damage);
  ReadMembers<ReadBool>(exe, common.nobject_types, &NObjectType::worm_explode);
  ReadMembers<ReadBool>(exe, common.nobject_types, &NObjectType::expl_ground);
  ReadMembers<ReadBool>(exe, common.nobject_types, &NObjectType::worm_destroy);
  ReadMembers<Read8>(exe, common.nobject_types, &NObjectType::blood_on_hit);
  ReadMembers<Read8>(exe, common.nobject_types, &NObjectType::start_frame);
  ReadMembers<Read8>(exe, common.nobject_types, &NObjectType::num_frames);
  ReadMembers<ReadBool>(exe, common.nobject_types, &NObjectType::draw_on_map);
  ReadMembers<Read8>(exe, common.nobject_types, &NObjectType::color_bullets);
  ReadMembers<Dec<Read8> >(exe, common.nobject_types, &NObjectType::create_on_exp);
  ReadMembers<ReadBool>(exe, common.nobject_types, &NObjectType::affect_by_explosions);
  ReadMembers<Dec<Read8> >(exe, common.nobject_types, &NObjectType::dirt_effect);
  ReadMembers<Read8>(exe, common.nobject_types, &NObjectType::splinter_amount);
  ReadMembers<Read8>(exe, common.nobject_types, &NObjectType::splinter_colour);
  ReadMembers<Dec<Read8> >(exe, common.nobject_types, &NObjectType::splinter_type);
  ReadMembers<ReadBool>(exe, common.nobject_types, &NObjectType::blood_trail);
  ReadMembers<Read8>(exe, common.nobject_types, &NObjectType::blood_trail_delay);
  ReadMembers<Dec<Read8> >(exe, common.nobject_types, &NObjectType::leave_obj);
  ReadMembers<Read8>(exe, common.nobject_types, &NObjectType::leave_obj_delay);
  ReadMembers<Read16>(exe, common.nobject_types, &NObjectType::time_to_explo);
  ReadMembers<Read16>(exe, common.nobject_types, &NObjectType::time_to_explo_v);

  for (int i = 0; i < 24; ++i)  // TODO: Unhardcode
  {
    common.nobject_types[i].id = i;
  }
}

static void LoadTextures(Common& common, ReaderFile& exe) {
  exe.Seekg(0x1C208);
  ReadMembers<ReadBool>(exe, common.textures, &Texture::n_draw_back);
  exe.Seekg(0x1C1EA);
  ReadMembers<Read8>(exe, common.textures, &Texture::m_frame);
  exe.Seekg(0x1C1F4);
  ReadMembers<Read8>(exe, common.textures, &Texture::s_frame);
  exe.Seekg(0x1C1FE);
  ReadMembers<Read8>(exe, common.textures, &Texture::r_frame);
}

static void LoadOthers(Common& common, ReaderFile& exe) {
  exe.Seekg(0x1C1E2);

  for (int i = 0; i < 2; ++i)
    for (auto& j : common.bonus_rand_timer) j[i] = io::ReadUint16Le(exe);

  exe.Seekg(0x1AEEE + 2);

  for (auto& i : common.ai_params.k)
    for (int& j : i) j = io::ReadUint16Le(exe);

  exe.Seekg(0x1C1E0);

  for (int& bonus_s_object : common.bonus_s_objects) bonus_s_object = exe.Get() - 1;
}

static void LoadSprites(SpriteSet& ss, ReaderFile& f, int width, int height, int count) {
  assert(width == height);  // We only support rectangular sprites right now

  ss.width = width;
  ss.height = height;
  ss.sprite_size = width * height;
  ss.count = count;

  int const kAmount = ss.sprite_size * count;
  ss.data.resize(kAmount);

  std::vector<uint8_t> temp(kAmount);

  f.Get(temp.data(), kAmount);

  PalIdx* dest = ss.data.data();
  uint8_t const* src = temp.data();

  for (int i = 0; i < count; i++) {
    for (int x = 0; x < width; ++x) {
      for (int y = 0; y < height; ++y) {
        dest[x + y * width] = src[y];
      }

      src += height;
    }

    dest += ss.sprite_size;
  }
}

static void CropSprites(SpriteSet& sprites, int first, int count, int min_x, int min_y, int width,
                        int height) {
  // Crop sprites by clearing pixels outside the desired area.

  int const kMaxX = min_x + width - 1;
  int const kMaxY = min_y + height - 1;

  for (int i = first; i < first + count; i++) {
    Sprite const kSprite = sprites[i];

    for (int y = 0; y < kSprite.height; y++) {
      for (int x = 0; x < kSprite.width; x++) {
        if (x < min_x || x > kMaxX || y < min_y || y > kMaxY)
          kSprite.mem[y * kSprite.width + x] = 0;
      }
    }
  }
}

static void LoadGfx(Common& common, ReaderFile& exe, ReaderFile& gfx) {
  exe.Seekg(0x1C1DE);
  common.bonus_frames[0] = exe.Get();
  common.bonus_frames[1] = exe.Get();

  gfx.Seekg(10);  // Skip some header

  LoadSprites(common.large_sprites, gfx, 16, 16, 110);
  gfx.Skip(4);  // Extra stuff

  LoadSprites(common.small_sprites, gfx, 7, 7, 130);
  gfx.Skip(4);  // Extra stuff

  LoadSprites(common.text_sprites, gfx, 4, 4, 26);

  // The original would only render 10x9 pixels of the worm sprites.
  // Cropping the worm sprites here to match the original behavior.
  CropSprites(common.large_sprites, 16, 21, 2, 0, 10, 9);

  Rand rand;

  for (int y = 0; y < 16; ++y)
    for (int x = 0; x < 16; ++x) {
      int const kIdx = y * 16 + x;
      common.large_sprites.SpritePtr(73)[kIdx] = rand(4) + 160;
      common.large_sprites.SpritePtr(74)[kIdx] = rand(4) + 160;

      common.large_sprites.SpritePtr(87)[kIdx] = rand(4) + 12;
      common.large_sprites.SpritePtr(88)[kIdx] = rand(4) + 12;

      common.large_sprites.SpritePtr(82)[kIdx] = rand(4) + 94;
      common.large_sprites.SpritePtr(83)[kIdx] = rand(4) + 94;
    }
}

void LoadSfx(std::vector<SfxSample>& sounds, ReaderFile& snd) {
  int const kCount = io::ReadUint16Le(snd);

  sounds.clear();

  for (int i = 0; i < kCount; ++i) {
    uint8_t name[9];
    name[8] = 0;
    snd.Get(name, 8);

    int const kOffset = io::ReadUint32Le(snd);
    int const kLength = io::ReadUint32Le(snd);

    auto old_pos = snd.Tellg();

    SfxSample sample(ToId(reinterpret_cast<char const*>(name)), kLength);

    if (kLength > 0) {
      snd.Seekg(kOffset);
      snd.Get(sample.original_data.data(), kLength);

      sample.CreateSound();
    }

    snd.Seekg(old_pos);

    sounds.push_back(std::move(sample));
  }
}

static void LoadFont(Font& font, ReaderFile& exe) {
  font.chars.resize(250);

  std::size_t const kFontSize = 250 * 8 * 8 + 1;
  std::vector<unsigned char> temp(kFontSize);

  exe.Seekg(0x1C825);

  exe.Get(reinterpret_cast<uint8_t*>(temp.data()), kFontSize);

  for (int i = 0; i < 250; ++i) {
    unsigned char const* ptr = &temp[i * 64 + 1];

    for (int y = 0; y < 8; ++y) {
      for (int x = 0; x < 7; ++x) {
        font.chars[i].data[y * 7 + x] = ptr[y * 8 + x];
      }
    }

    font.chars[i].width = ptr[63];
  }
}

void LoadFromExe(Common& common, ReaderFile& exe, ReaderFile& gfx, ReaderFile& snd) {
  common.weapons.resize(40);
  common.nobject_types.resize(24);
  common.sobject_types.resize(14);

  for (int i = 0; i < 14; ++i) {
    common.sobject_types[i].id_str = ToId(sobject_names[i]);
  }

  for (int i = 0; i < 24; ++i) {
    common.nobject_types[i].id_str = ToId(nobject_names[i]);
  }

  LoadConstants(common, exe);
  LoadFont(common.font, exe);
  LoadPalette(common, exe);
  LoadMaterials(common, exe);
  LoadWeapons(common, exe);
  LoadTextures(common, exe);
  LoadOthers(common, exe);

  LoadGfx(common, exe, gfx);
  LoadSfx(common.sounds, snd);

  // Resolve the engine's named sound hooks against the loaded sound
  // table so saveTcConfig emits a populated [sounds] section. Without
  // this, a freshly extracted TC has every hook at -1 and the engine's
  // menu / round-begin / bump / reload sounds are silent (issue #44).
  static struct {
    SoundDefT hook;
    char const* name;
  } const kHookNames[] = {
      {.hook = SoundMenuMoveUp, .name = "moveup"}, {.hook = SoundMenuMoveDown, .name = "movedown"},
      {.hook = SoundMenuSelect, .name = "select"}, {.hook = SoundBump, .name = "bump"},
      {.hook = SoundBegin, .name = "begin"},       {.hook = SoundReloaded, .name = "reloaded"},
      {.hook = SoundAlive, .name = "alive"},       {.hook = SoundNinjaropeThrow, .name = "throw"},
  };
  for (auto const& m : kHookNames) common.sound_hook[m.hook] = common.SoundIndex(m.name);
}
