#pragma once

#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <sstream>
#include <string>
#include <vector>

#include "common.hpp"
#include "console.hpp"
#include "serialization/toml_archive.hpp"

// Cross-reference resolution helpers
template <typename T>
inline std::string objRefToStr(int idx, std::vector<T> const& vec) {
  if (idx < 0 || idx >= (int)vec.size()) return "";
  return vec[idx].idStr;
}

template <typename T>
inline int objRefFromStr(std::string const& str, std::vector<T> const& vec) {
  if (str.empty()) return -1;
  for (std::size_t i = 0; i < vec.size(); ++i)
    if (vec[i].idStr == str) return (int)i;
  return 0;
}

// Sound-ref helpers: distinct from objRefFromStr because an unknown
// non-empty name must resolve to -1 (no sound), not 0 (would spuriously
// play the first sound).
inline std::string soundRefToStr(int idx, Common const& common) {
  if (idx < 0 || idx >= (int)common.sounds.size()) return "";
  return common.sounds[idx].name;
}

inline int soundRefFromStr(std::string const& str, Common const& common) {
  if (str.empty()) return -1;
  return common.soundIndex(str);
}

// Save/load NObjectType config (individual .cfg file)
inline void saveNObjectConfig(Common const& common, NObjectType const& n, std::ostream& os) {
  cereal::TomlOutputArchive ar(os);
  ar(cereal::make_nvp("wormExplode", n.wormExplode));
  ar(cereal::make_nvp("explGround", n.explGround));
  ar(cereal::make_nvp("wormDestroy", n.wormDestroy));
  ar(cereal::make_nvp("drawOnMap", n.drawOnMap));
  ar(cereal::make_nvp("affectByExplosions", n.affectByExplosions));
  ar(cereal::make_nvp("bloodTrail", n.bloodTrail));
  ar(cereal::make_nvp("detectDistance", n.detectDistance));
  ar(cereal::make_nvp("gravity", n.gravity));
  ar(cereal::make_nvp("speed", n.speed));
  ar(cereal::make_nvp("speedV", n.speedV));
  ar(cereal::make_nvp("distribution", n.distribution));
  ar(cereal::make_nvp("blowAway", n.blowAway));
  ar(cereal::make_nvp("bounce", n.bounce));
  ar(cereal::make_nvp("hitDamage", n.hitDamage));
  ar(cereal::make_nvp("bloodOnHit", n.bloodOnHit));
  ar(cereal::make_nvp("startFrame", n.startFrame));
  ar(cereal::make_nvp("numFrames", n.numFrames));
  ar(cereal::make_nvp("colorBullets", n.colorBullets));
  {
    std::string ref = objRefToStr(n.createOnExp, common.sobjectTypes);
    ar(cereal::make_nvp("createOnExp", ref));
  }
  ar(cereal::make_nvp("dirtEffect", n.dirtEffect));
  ar(cereal::make_nvp("splinterAmount", n.splinterAmount));
  ar(cereal::make_nvp("splinterColour", n.splinterColour));
  {
    std::string ref = objRefToStr(n.splinterType, common.nobjectTypes);
    ar(cereal::make_nvp("splinterType", ref));
  }
  ar(cereal::make_nvp("bloodTrailDelay", n.bloodTrailDelay));
  {
    std::string ref = objRefToStr(n.leaveObj, common.sobjectTypes);
    ar(cereal::make_nvp("leaveObj", ref));
  }
  ar(cereal::make_nvp("leaveObjDelay", n.leaveObjDelay));
  ar(cereal::make_nvp("timeToExplo", n.timeToExplo));
  ar(cereal::make_nvp("timeToExploV", n.timeToExploV));
}

inline void loadNObjectConfig(Common& common, NObjectType& n, std::istream& is) {
  cereal::TomlInputArchive ar(is);
  ar(cereal::make_nvp("wormExplode", n.wormExplode));
  ar(cereal::make_nvp("explGround", n.explGround));
  ar(cereal::make_nvp("wormDestroy", n.wormDestroy));
  ar(cereal::make_nvp("drawOnMap", n.drawOnMap));
  ar(cereal::make_nvp("affectByExplosions", n.affectByExplosions));
  ar(cereal::make_nvp("bloodTrail", n.bloodTrail));
  ar(cereal::make_nvp("detectDistance", n.detectDistance));
  ar(cereal::make_nvp("gravity", n.gravity));
  ar(cereal::make_nvp("speed", n.speed));
  ar(cereal::make_nvp("speedV", n.speedV));
  ar(cereal::make_nvp("distribution", n.distribution));
  ar(cereal::make_nvp("blowAway", n.blowAway));
  ar(cereal::make_nvp("bounce", n.bounce));
  ar(cereal::make_nvp("hitDamage", n.hitDamage));
  ar(cereal::make_nvp("bloodOnHit", n.bloodOnHit));
  ar(cereal::make_nvp("startFrame", n.startFrame));
  ar(cereal::make_nvp("numFrames", n.numFrames));
  ar(cereal::make_nvp("colorBullets", n.colorBullets));
  {
    std::string ref;
    ar(cereal::make_nvp("createOnExp", ref));
    n.createOnExp = objRefFromStr(ref, common.sobjectTypes);
  }
  ar(cereal::make_nvp("dirtEffect", n.dirtEffect));
  ar(cereal::make_nvp("splinterAmount", n.splinterAmount));
  ar(cereal::make_nvp("splinterColour", n.splinterColour));
  {
    std::string ref;
    ar(cereal::make_nvp("splinterType", ref));
    n.splinterType = objRefFromStr(ref, common.nobjectTypes);
  }
  ar(cereal::make_nvp("bloodTrailDelay", n.bloodTrailDelay));
  {
    std::string ref;
    ar(cereal::make_nvp("leaveObj", ref));
    n.leaveObj = objRefFromStr(ref, common.sobjectTypes);
  }
  ar(cereal::make_nvp("leaveObjDelay", n.leaveObjDelay));
  ar(cereal::make_nvp("timeToExplo", n.timeToExplo));
  ar(cereal::make_nvp("timeToExploV", n.timeToExploV));
}

// Save/load SObjectType config
inline void saveSObjectConfig(Common const& common, SObjectType const& s, std::ostream& os) {
  cereal::TomlOutputArchive ar(os);
  ar(cereal::make_nvp("shadow", s.shadow));
  {
    std::string ref = soundRefToStr(s.startSound, common);
    ar(cereal::make_nvp("startSound", ref));
  }
  ar(cereal::make_nvp("numSounds", s.numSounds));
  ar(cereal::make_nvp("animDelay", s.animDelay));
  ar(cereal::make_nvp("startFrame", s.startFrame));
  ar(cereal::make_nvp("numFrames", s.numFrames));
  ar(cereal::make_nvp("detectRange", s.detectRange));
  ar(cereal::make_nvp("damage", s.damage));
  ar(cereal::make_nvp("blowAway", s.blowAway));
  ar(cereal::make_nvp("shake", s.shake));
  ar(cereal::make_nvp("flash", s.flash));
  ar(cereal::make_nvp("dirtEffect", s.dirtEffect));
}

inline void loadSObjectConfig(Common const& common, SObjectType& s, std::istream& is) {
  cereal::TomlInputArchive ar(is);
  ar(cereal::make_nvp("shadow", s.shadow));
  {
    std::string ref;
    ar(cereal::make_nvp("startSound", ref));
    s.startSound = soundRefFromStr(ref, common);
  }
  ar(cereal::make_nvp("numSounds", s.numSounds));
  ar(cereal::make_nvp("animDelay", s.animDelay));
  ar(cereal::make_nvp("startFrame", s.startFrame));
  ar(cereal::make_nvp("numFrames", s.numFrames));
  ar(cereal::make_nvp("detectRange", s.detectRange));
  ar(cereal::make_nvp("damage", s.damage));
  ar(cereal::make_nvp("blowAway", s.blowAway));
  ar(cereal::make_nvp("shake", s.shake));
  ar(cereal::make_nvp("flash", s.flash));
  ar(cereal::make_nvp("dirtEffect", s.dirtEffect));
}

// Save/load Weapon config
inline void saveWeaponConfig(Common const& common, Weapon const& w, std::ostream& os) {
  cereal::TomlOutputArchive ar(os);
  ar(cereal::make_nvp("name", w.name));
  ar(cereal::make_nvp("affectByWorm", w.affectByWorm));
  ar(cereal::make_nvp("shadow", w.shadow));
  ar(cereal::make_nvp("laserSight", w.laserSight));
  ar(cereal::make_nvp("playReloadSound", w.playReloadSound));
  ar(cereal::make_nvp("wormExplode", w.wormExplode));
  ar(cereal::make_nvp("explGround", w.explGround));
  ar(cereal::make_nvp("wormCollide", w.wormCollide));
  ar(cereal::make_nvp("collideWithObjects", w.collideWithObjects));
  ar(cereal::make_nvp("affectByExplosions", w.affectByExplosions));
  ar(cereal::make_nvp("loopAnim", w.loopAnim));
  ar(cereal::make_nvp("detectDistance", w.detectDistance));
  ar(cereal::make_nvp("blowAway", w.blowAway));
  ar(cereal::make_nvp("gravity", w.gravity));
  {
    std::string ref = soundRefToStr(w.launchSound, common);
    ar(cereal::make_nvp("launchSound", ref));
  }
  {
    std::string ref = soundRefToStr(w.loopSound, common);
    ar(cereal::make_nvp("loopSound", ref));
  }
  {
    std::string ref = soundRefToStr(w.exploSound, common);
    ar(cereal::make_nvp("exploSound", ref));
  }
  ar(cereal::make_nvp("speed", w.speed));
  ar(cereal::make_nvp("addSpeed", w.addSpeed));
  ar(cereal::make_nvp("distribution", w.distribution));
  ar(cereal::make_nvp("parts", w.parts));
  ar(cereal::make_nvp("recoil", w.recoil));
  ar(cereal::make_nvp("multSpeed", w.multSpeed));
  ar(cereal::make_nvp("delay", w.delay));
  ar(cereal::make_nvp("loadingTime", w.loadingTime));
  ar(cereal::make_nvp("ammo", w.ammo));
  ar(cereal::make_nvp("dirtEffect", w.dirtEffect));
  ar(cereal::make_nvp("leaveShells", w.leaveShells));
  ar(cereal::make_nvp("leaveShellDelay", w.leaveShellDelay));
  ar(cereal::make_nvp("fireCone", w.fireCone));
  ar(cereal::make_nvp("bounce", w.bounce));
  ar(cereal::make_nvp("timeToExplo", w.timeToExplo));
  ar(cereal::make_nvp("timeToExploV", w.timeToExploV));
  ar(cereal::make_nvp("hitDamage", w.hitDamage));
  ar(cereal::make_nvp("bloodOnHit", w.bloodOnHit));
  ar(cereal::make_nvp("startFrame", w.startFrame));
  ar(cereal::make_nvp("numFrames", w.numFrames));
  ar(cereal::make_nvp("shotType", w.shotType));
  ar(cereal::make_nvp("colorBullets", w.colorBullets));
  ar(cereal::make_nvp("splinterAmount", w.splinterAmount));
  ar(cereal::make_nvp("splinterColour", w.splinterColour));
  {
    std::string ref = objRefToStr(w.splinterType, common.nobjectTypes);
    ar(cereal::make_nvp("splinterType", ref));
  }
  ar(cereal::make_nvp("splinterScatter", w.splinterScatter));
  {
    std::string ref = objRefToStr(w.objTrailType, common.sobjectTypes);
    ar(cereal::make_nvp("objTrailType", ref));
  }
  ar(cereal::make_nvp("objTrailDelay", w.objTrailDelay));
  ar(cereal::make_nvp("partTrailType", w.partTrailType));
  {
    std::string ref = objRefToStr(w.partTrailObj, common.nobjectTypes);
    ar(cereal::make_nvp("partTrailObj", ref));
  }
  ar(cereal::make_nvp("partTrailDelay", w.partTrailDelay));
  {
    std::string ref = objRefToStr(w.createOnExp, common.sobjectTypes);
    ar(cereal::make_nvp("createOnExp", ref));
  }
  ar(cereal::make_nvp("chainExplosion", w.chainExplosion));
}

inline void loadWeaponConfig(Common& common, Weapon& w, std::istream& is) {
  cereal::TomlInputArchive ar(is);
  ar(cereal::make_nvp("name", w.name));
  ar(cereal::make_nvp("affectByWorm", w.affectByWorm));
  ar(cereal::make_nvp("shadow", w.shadow));
  ar(cereal::make_nvp("laserSight", w.laserSight));
  ar(cereal::make_nvp("playReloadSound", w.playReloadSound));
  ar(cereal::make_nvp("wormExplode", w.wormExplode));
  ar(cereal::make_nvp("explGround", w.explGround));
  ar(cereal::make_nvp("wormCollide", w.wormCollide));
  ar(cereal::make_nvp("collideWithObjects", w.collideWithObjects));
  ar(cereal::make_nvp("affectByExplosions", w.affectByExplosions));
  ar(cereal::make_nvp("loopAnim", w.loopAnim));
  ar(cereal::make_nvp("detectDistance", w.detectDistance));
  ar(cereal::make_nvp("blowAway", w.blowAway));
  ar(cereal::make_nvp("gravity", w.gravity));
  {
    std::string ref;
    ar(cereal::make_nvp("launchSound", ref));
    w.launchSound = soundRefFromStr(ref, common);
  }
  {
    std::string ref;
    ar(cereal::make_nvp("loopSound", ref));
    w.loopSound = soundRefFromStr(ref, common);
  }
  {
    std::string ref;
    ar(cereal::make_nvp("exploSound", ref));
    w.exploSound = soundRefFromStr(ref, common);
  }
  ar(cereal::make_nvp("speed", w.speed));
  ar(cereal::make_nvp("addSpeed", w.addSpeed));
  ar(cereal::make_nvp("distribution", w.distribution));
  ar(cereal::make_nvp("parts", w.parts));
  ar(cereal::make_nvp("recoil", w.recoil));
  ar(cereal::make_nvp("multSpeed", w.multSpeed));
  ar(cereal::make_nvp("delay", w.delay));
  ar(cereal::make_nvp("loadingTime", w.loadingTime));
  ar(cereal::make_nvp("ammo", w.ammo));
  ar(cereal::make_nvp("dirtEffect", w.dirtEffect));
  ar(cereal::make_nvp("leaveShells", w.leaveShells));
  ar(cereal::make_nvp("leaveShellDelay", w.leaveShellDelay));
  ar(cereal::make_nvp("fireCone", w.fireCone));
  ar(cereal::make_nvp("bounce", w.bounce));
  ar(cereal::make_nvp("timeToExplo", w.timeToExplo));
  ar(cereal::make_nvp("timeToExploV", w.timeToExploV));
  ar(cereal::make_nvp("hitDamage", w.hitDamage));
  ar(cereal::make_nvp("bloodOnHit", w.bloodOnHit));
  ar(cereal::make_nvp("startFrame", w.startFrame));
  ar(cereal::make_nvp("numFrames", w.numFrames));
  ar(cereal::make_nvp("shotType", w.shotType));
  ar(cereal::make_nvp("colorBullets", w.colorBullets));
  ar(cereal::make_nvp("splinterAmount", w.splinterAmount));
  ar(cereal::make_nvp("splinterColour", w.splinterColour));
  {
    std::string ref;
    ar(cereal::make_nvp("splinterType", ref));
    w.splinterType = objRefFromStr(ref, common.nobjectTypes);
  }
  ar(cereal::make_nvp("splinterScatter", w.splinterScatter));
  {
    std::string ref;
    ar(cereal::make_nvp("objTrailType", ref));
    w.objTrailType = objRefFromStr(ref, common.sobjectTypes);
  }
  ar(cereal::make_nvp("objTrailDelay", w.objTrailDelay));
  ar(cereal::make_nvp("partTrailType", w.partTrailType));
  {
    std::string ref;
    ar(cereal::make_nvp("partTrailObj", ref));
    w.partTrailObj = objRefFromStr(ref, common.nobjectTypes);
  }
  ar(cereal::make_nvp("partTrailDelay", w.partTrailDelay));
  {
    std::string ref;
    ar(cereal::make_nvp("createOnExp", ref));
    w.createOnExp = objRefFromStr(ref, common.sobjectTypes);
  }
  ar(cereal::make_nvp("chainExplosion", w.chainExplosion));
}

// --- Helper structs for tc.cfg serialization (must be at file scope for templates) ---
namespace tc_cfg {

struct Types {
  std::vector<std::string> sounds;
  std::vector<std::string> weapons;
  std::vector<std::string> nobjects;
  std::vector<std::string> sobjects;
  template <typename Archive>
  void serialize(Archive& ar) {
    ar(cereal::make_nvp("sounds", sounds));
    ar(cereal::make_nvp("weapons", weapons));
    ar(cereal::make_nvp("nobjects", nobjects));
    ar(cereal::make_nvp("sobjects", sobjects));
  }
};

struct BonusEntry {
  int32_t timer = 0;
  int32_t timerV = 0;
  int32_t frame = 0;
  std::string sobj;
  template <typename Archive>
  void serialize(Archive& ar) {
    ar(cereal::make_nvp("timer", timer));
    ar(cereal::make_nvp("timerV", timerV));
    ar(cereal::make_nvp("frame", frame));
    ar(cereal::make_nvp("sobj", sobj));
  }
};

struct TextureEntry {
  int32_t mframe = 0;
  int32_t rframe = 0;
  int32_t sframe = 0;
  bool ndrawback = false;
  template <typename Archive>
  void serialize(Archive& ar) {
    ar(cereal::make_nvp("mframe", mframe));
    ar(cereal::make_nvp("rframe", rframe));
    ar(cereal::make_nvp("sframe", sframe));
    ar(cereal::make_nvp("ndrawback", ndrawback));
  }
};

struct ColourAnimEntry {
  int32_t from = 0;
  int32_t to = 0;
  template <typename Archive>
  void serialize(Archive& ar) {
    ar(cereal::make_nvp("from", from));
    ar(cereal::make_nvp("to", to));
  }
};

struct AiKey {
  int32_t on = 0;
  int32_t off = 0;
  template <typename Archive>
  void serialize(Archive& ar) {
    ar(cereal::make_nvp("on", on));
    ar(cereal::make_nvp("off", off));
  }
};

struct AiParamsS {
  AiKey up, down, left, right, fire, change, jump;
  template <typename Archive>
  void serialize(Archive& ar) {
    ar(cereal::make_nvp("up", up));
    ar(cereal::make_nvp("down", down));
    ar(cereal::make_nvp("left", left));
    ar(cereal::make_nvp("right", right));
    ar(cereal::make_nvp("fire", fire));
    ar(cereal::make_nvp("change", change));
    ar(cereal::make_nvp("jump", jump));
  }
};

struct Constants {
  std::vector<BonusEntry> bonuses;
  std::vector<TextureEntry> textures;
  std::vector<ColourAnimEntry> colorAnim;
  std::vector<int32_t> materials;
  AiParamsS aiparams;
#define DECL_FIELD_C(n) int32_t n = 0;
  LIERO_CDEFS(DECL_FIELD_C)
#undef DECL_FIELD_C

  template <typename Archive>
  void serialize(Archive& ar) {
    ar(cereal::make_nvp("bonuses", bonuses));
    ar(cereal::make_nvp("textures", textures));
    ar(cereal::make_nvp("colorAnim", colorAnim));
    ar(cereal::make_nvp("materials", materials));
    ar(cereal::make_nvp("aiparams", aiparams));
#define SER_FIELD_C(n) ar(cereal::make_nvp(#n, n));
    LIERO_CDEFS(SER_FIELD_C)
#undef SER_FIELD_C
  }
};

struct TextsS {
#define DECL_FIELD_S(n) std::string n;
  LIERO_SDEFS(DECL_FIELD_S)
#undef DECL_FIELD_S
  template <typename Archive>
  void serialize(Archive& ar) {
#define SER_FIELD_S(n) ar(cereal::make_nvp(#n, n));
    LIERO_SDEFS(SER_FIELD_S)
#undef SER_FIELD_S
  }
};

struct Hacks {
#define DECL_FIELD_H(n) bool n = false;
  LIERO_HDEFS(DECL_FIELD_H)
#undef DECL_FIELD_H
  template <typename Archive>
  void serialize(Archive& ar) {
#define SER_FIELD_H(n) ar(cereal::make_nvp(#n, n));
    LIERO_HDEFS(SER_FIELD_H)
#undef SER_FIELD_H
  }
};

struct Sounds {
#define DECL_FIELD_SO(n) std::string n;
  LIERO_SOUNDDEFS(DECL_FIELD_SO)
#undef DECL_FIELD_SO
  template <typename Archive>
  void serialize(Archive& ar) {
#define SER_FIELD_SO(n) ar(cereal::make_nvp(#n, n));
    LIERO_SOUNDDEFS(SER_FIELD_SO)
#undef SER_FIELD_SO
  }
};

}  // namespace tc_cfg

// Save/load tc.cfg (top-level Common config)
inline void saveTcConfig(Common const& common, std::ostream& os) {
  tc_cfg::Types types;
  for (auto& s : common.sounds) types.sounds.push_back(s.name);
  for (auto& w : common.weapons) types.weapons.push_back(w.idStr);
  for (auto& n : common.nobjectTypes) types.nobjects.push_back(n.idStr);
  for (auto& s : common.sobjectTypes) types.sobjects.push_back(s.idStr);

  tc_cfg::Constants constants;
  for (int i = 0; i < NUM_BONUS_SOBJECTS; ++i) {
    tc_cfg::BonusEntry be;
    be.timer = common.bonusRandTimer[i][0];
    be.timerV = common.bonusRandTimer[i][1];
    be.frame = common.bonusFrames[i];
    be.sobj = objRefToStr(common.bonusSObjects[i], common.sobjectTypes);
    constants.bonuses.push_back(be);
  }
  for (int i = 0; i < NUM_TEXTURES; ++i) {
    tc_cfg::TextureEntry te;
    te.mframe = common.textures[i].mFrame;
    te.rframe = common.textures[i].rFrame;
    te.sframe = common.textures[i].sFrame;
    te.ndrawback = common.textures[i].nDrawBack;
    constants.textures.push_back(te);
  }
  for (int i = 0; i < NUM_COLOR_ANIM; ++i) {
    tc_cfg::ColourAnimEntry ce;
    ce.from = common.colorAnim[i].from;
    ce.to = common.colorAnim[i].to;
    constants.colorAnim.push_back(ce);
  }
  for (int i = 0; i < MAX_MATERIALS; ++i) constants.materials.push_back(common.materials[i].flags);

  auto fillAiKey = [&](tc_cfg::AiKey& k, int idx) {
    k.on = common.aiParams.k[1][idx];
    k.off = common.aiParams.k[0][idx];
  };
  fillAiKey(constants.aiparams.up, 0);
  fillAiKey(constants.aiparams.down, 1);
  fillAiKey(constants.aiparams.left, 2);
  fillAiKey(constants.aiparams.right, 3);
  fillAiKey(constants.aiparams.fire, 4);
  fillAiKey(constants.aiparams.change, 5);
  fillAiKey(constants.aiparams.jump, 6);

#define COPY_FIELD_C(n) constants.n = common.C[C##n];
  LIERO_CDEFS(COPY_FIELD_C)
#undef COPY_FIELD_C

  tc_cfg::TextsS texts;
#define COPY_FIELD_S(n) texts.n = common.S[S##n];
  LIERO_SDEFS(COPY_FIELD_S)
#undef COPY_FIELD_S

  tc_cfg::Hacks hacks;
#define COPY_FIELD_H(n) hacks.n = common.H[H##n];
  LIERO_HDEFS(COPY_FIELD_H)
#undef COPY_FIELD_H

  tc_cfg::Sounds sounds;
#define COPY_FIELD_SO(n)                                                                          \
  sounds.n =                                                                                      \
      (common.soundHook[Sound##n] >= 0 && common.soundHook[Sound##n] < (int)common.sounds.size()) \
          ? common.sounds[common.soundHook[Sound##n]].name                                        \
          : std::string();
  LIERO_SOUNDDEFS(COPY_FIELD_SO)
#undef COPY_FIELD_SO

  cereal::TomlOutputArchive ar(os);
  ar(cereal::make_nvp("types", types));
  ar(cereal::make_nvp("constants", constants));
  ar(cereal::make_nvp("texts", texts));
  ar(cereal::make_nvp("hacks", hacks));
  ar(cereal::make_nvp("sounds", sounds));
}

inline void loadTcConfig(Common& common, std::istream& is) {
  tc_cfg::Types types;
  tc_cfg::Constants constants;
  tc_cfg::TextsS texts;
  tc_cfg::Hacks hacks;
  tc_cfg::Sounds sounds;

  cereal::TomlInputArchive ar(is);
  ar(cereal::make_nvp("types", types));
  ar(cereal::make_nvp("constants", constants));
  ar(cereal::make_nvp("texts", texts));
  ar(cereal::make_nvp("hacks", hacks));
  ar(cereal::make_nvp("sounds", sounds));

  // Populate Common from deserialized structs
  common.sounds.clear();
  common.sounds.resize(types.sounds.size());
  for (std::size_t i = 0; i < types.sounds.size(); ++i) common.sounds[i].name = types.sounds[i];

  common.weapons.resize(types.weapons.size());
  for (std::size_t i = 0; i < types.weapons.size(); ++i) common.weapons[i].idStr = types.weapons[i];

  common.nobjectTypes.resize(types.nobjects.size());
  for (std::size_t i = 0; i < types.nobjects.size(); ++i)
    common.nobjectTypes[i].idStr = types.nobjects[i];

  common.sobjectTypes.resize(types.sobjects.size());
  for (std::size_t i = 0; i < types.sobjects.size(); ++i)
    common.sobjectTypes[i].idStr = types.sobjects[i];

  // Constants
  for (std::size_t i = 0; i < constants.bonuses.size() && i < NUM_BONUS_SOBJECTS; ++i) {
    common.bonusRandTimer[i][0] = constants.bonuses[i].timer;
    common.bonusRandTimer[i][1] = constants.bonuses[i].timerV;
    common.bonusFrames[i] = constants.bonuses[i].frame;
    common.bonusSObjects[i] = objRefFromStr(constants.bonuses[i].sobj, common.sobjectTypes);
  }

  for (std::size_t i = 0; i < constants.textures.size() && i < NUM_TEXTURES; ++i) {
    common.textures[i].mFrame = constants.textures[i].mframe;
    common.textures[i].rFrame = constants.textures[i].rframe;
    common.textures[i].sFrame = constants.textures[i].sframe;
    common.textures[i].nDrawBack = constants.textures[i].ndrawback;
  }

  for (std::size_t i = 0; i < constants.colorAnim.size() && i < NUM_COLOR_ANIM; ++i) {
    common.colorAnim[i].from = constants.colorAnim[i].from;
    common.colorAnim[i].to = constants.colorAnim[i].to;
  }

  for (std::size_t i = 0; i < constants.materials.size() && i < MAX_MATERIALS; ++i) {
    common.materials[i].flags = (uint8_t)(constants.materials[i] & 0xff);
  }

  auto readAiKey = [&](tc_cfg::AiKey const& k, int idx) {
    common.aiParams.k[1][idx] = k.on;
    common.aiParams.k[0][idx] = k.off;
  };
  readAiKey(constants.aiparams.up, 0);
  readAiKey(constants.aiparams.down, 1);
  readAiKey(constants.aiparams.left, 2);
  readAiKey(constants.aiparams.right, 3);
  readAiKey(constants.aiparams.fire, 4);
  readAiKey(constants.aiparams.change, 5);
  readAiKey(constants.aiparams.jump, 6);

#define COPY_FIELD_C2(n) common.C[C##n] = constants.n;
  LIERO_CDEFS(COPY_FIELD_C2)
#undef COPY_FIELD_C2

#define COPY_FIELD_S2(n) common.S[S##n] = texts.n;
  LIERO_SDEFS(COPY_FIELD_S2)
#undef COPY_FIELD_S2

#define COPY_FIELD_H2(n) common.H[H##n] = hacks.n;
  LIERO_HDEFS(COPY_FIELD_H2)
#undef COPY_FIELD_H2

  // Resolve [sounds] hook names against the now-loaded sound table.
  // Missing/empty entries fall back to the canonical Liero sound name
  // for that hook (so TCs whose tc.cfg predates the [sounds] section
  // still get menu/round/bump/reload sounds). Names that don't match
  // any loaded sound get a warning and resolve to -1.
  auto resolveHook = [&](SOUND_DEF_T hook, std::string const& configured, char const* defaultName,
                         char const* hookLabel) {
    if (!configured.empty()) {
      int idx = common.soundIndex(configured);
      if (idx < 0)
        Console::writeWarning(std::string("[sounds] ") + hookLabel +
                              " references unknown sound \"" + configured + "\"");
      common.soundHook[hook] = idx;
    } else {
      common.soundHook[hook] = common.soundIndex(defaultName);
    }
  };
#define COPY_FIELD_SO2(n) resolveHook(Sound##n, sounds.n, defaultSound##n, #n);
  // Canonical Liero sound names per hook. Kept in sync with tctool's
  // extracted sound table; serves as the fallback when [sounds] is
  // missing from tc.cfg.
  constexpr char const* defaultSoundMenuMoveUp = "moveup";
  constexpr char const* defaultSoundMenuMoveDown = "movedown";
  constexpr char const* defaultSoundMenuSelect = "select";
  constexpr char const* defaultSoundBump = "bump";
  constexpr char const* defaultSoundBegin = "begin";
  constexpr char const* defaultSoundReloaded = "reloaded";
  constexpr char const* defaultSoundAlive = "alive";
  constexpr char const* defaultSoundNinjaropeThrow = "throw";
  LIERO_SOUNDDEFS(COPY_FIELD_SO2)
#undef COPY_FIELD_SO2
}
