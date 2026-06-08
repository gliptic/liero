#pragma once

#include <cereal/cereal.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "common.hpp"
#include "console.hpp"
#include "serialization/toml_archive.hpp"

// Cross-reference resolution helpers
template <typename T>
inline std::string ObjRefToStr(int idx, std::vector<T> const& vec) {
  if (idx < 0 || idx >= static_cast<int>(vec.size())) return "";
  return vec[idx].id_str;
}

template <typename T>
inline int ObjRefFromStr(std::string const& str, std::vector<T> const& vec) {
  if (str.empty()) return -1;
  for (std::size_t i = 0; i < vec.size(); ++i)
    if (vec[i].id_str == str) return static_cast<int>(i);
  return 0;
}

// Sound-ref helpers: distinct from objRefFromStr because an unknown
// non-empty name must resolve to -1 (no sound), not 0 (would spuriously
// play the first sound).
inline std::string SoundRefToStr(int idx, Common const& common) {
  if (idx < 0 || std::cmp_greater_equal(idx, common.sounds.size())) return "";
  return common.sounds[idx].name;
}

inline int SoundRefFromStr(std::string const& str, Common const& common) {
  if (str.empty()) return -1;
  return common.SoundIndex(str);
}

// Save/load NObjectType config (individual .cfg file)
inline void SaveNObjectConfig(Common const& common, NObjectType const& n, std::ostream& os) {
  cereal::TomlOutputArchive ar(os);
  ar(cereal::make_nvp("wormExplode", n.worm_explode));
  ar(cereal::make_nvp("explGround", n.expl_ground));
  ar(cereal::make_nvp("wormDestroy", n.worm_destroy));
  ar(cereal::make_nvp("drawOnMap", n.draw_on_map));
  ar(cereal::make_nvp("affectByExplosions", n.affect_by_explosions));
  ar(cereal::make_nvp("bloodTrail", n.blood_trail));
  ar(cereal::make_nvp("detectDistance", n.detect_distance));
  ar(cereal::make_nvp("gravity", n.gravity));
  ar(cereal::make_nvp("speed", n.speed));
  ar(cereal::make_nvp("speedV", n.speed_v));
  ar(cereal::make_nvp("distribution", n.distribution));
  ar(cereal::make_nvp("blowAway", n.blow_away));
  ar(cereal::make_nvp("bounce", n.bounce));
  ar(cereal::make_nvp("hitDamage", n.hit_damage));
  ar(cereal::make_nvp("bloodOnHit", n.blood_on_hit));
  ar(cereal::make_nvp("startFrame", n.start_frame));
  ar(cereal::make_nvp("numFrames", n.num_frames));
  ar(cereal::make_nvp("colorBullets", n.color_bullets));
  {
    std::string ref = ObjRefToStr(n.create_on_exp, common.sobject_types);
    ar(cereal::make_nvp("createOnExp", ref));
  }
  ar(cereal::make_nvp("dirtEffect", n.dirt_effect));
  ar(cereal::make_nvp("splinterAmount", n.splinter_amount));
  ar(cereal::make_nvp("splinterColour", n.splinter_colour));
  {
    std::string ref = ObjRefToStr(n.splinter_type, common.nobject_types);
    ar(cereal::make_nvp("splinterType", ref));
  }
  ar(cereal::make_nvp("bloodTrailDelay", n.blood_trail_delay));
  {
    std::string ref = ObjRefToStr(n.leave_obj, common.sobject_types);
    ar(cereal::make_nvp("leaveObj", ref));
  }
  ar(cereal::make_nvp("leaveObjDelay", n.leave_obj_delay));
  ar(cereal::make_nvp("timeToExplo", n.time_to_explo));
  ar(cereal::make_nvp("timeToExploV", n.time_to_explo_v));
}

inline void LoadNObjectConfig(Common& common, NObjectType& n, std::istream& is) {
  cereal::TomlInputArchive ar(is);
  ar(cereal::make_nvp("wormExplode", n.worm_explode));
  ar(cereal::make_nvp("explGround", n.expl_ground));
  ar(cereal::make_nvp("wormDestroy", n.worm_destroy));
  ar(cereal::make_nvp("drawOnMap", n.draw_on_map));
  ar(cereal::make_nvp("affectByExplosions", n.affect_by_explosions));
  ar(cereal::make_nvp("bloodTrail", n.blood_trail));
  ar(cereal::make_nvp("detectDistance", n.detect_distance));
  ar(cereal::make_nvp("gravity", n.gravity));
  ar(cereal::make_nvp("speed", n.speed));
  ar(cereal::make_nvp("speedV", n.speed_v));
  ar(cereal::make_nvp("distribution", n.distribution));
  ar(cereal::make_nvp("blowAway", n.blow_away));
  ar(cereal::make_nvp("bounce", n.bounce));
  ar(cereal::make_nvp("hitDamage", n.hit_damage));
  ar(cereal::make_nvp("bloodOnHit", n.blood_on_hit));
  ar(cereal::make_nvp("startFrame", n.start_frame));
  ar(cereal::make_nvp("numFrames", n.num_frames));
  ar(cereal::make_nvp("colorBullets", n.color_bullets));
  {
    std::string ref;
    ar(cereal::make_nvp("createOnExp", ref));
    n.create_on_exp = ObjRefFromStr(ref, common.sobject_types);
  }
  ar(cereal::make_nvp("dirtEffect", n.dirt_effect));
  ar(cereal::make_nvp("splinterAmount", n.splinter_amount));
  ar(cereal::make_nvp("splinterColour", n.splinter_colour));
  {
    std::string ref;
    ar(cereal::make_nvp("splinterType", ref));
    n.splinter_type = ObjRefFromStr(ref, common.nobject_types);
  }
  ar(cereal::make_nvp("bloodTrailDelay", n.blood_trail_delay));
  {
    std::string ref;
    ar(cereal::make_nvp("leaveObj", ref));
    n.leave_obj = ObjRefFromStr(ref, common.sobject_types);
  }
  ar(cereal::make_nvp("leaveObjDelay", n.leave_obj_delay));
  ar(cereal::make_nvp("timeToExplo", n.time_to_explo));
  ar(cereal::make_nvp("timeToExploV", n.time_to_explo_v));
}

// Save/load SObjectType config
inline void SaveSObjectConfig(Common const& common, SObjectType const& s, std::ostream& os) {
  cereal::TomlOutputArchive ar(os);
  ar(cereal::make_nvp("shadow", s.shadow));
  {
    std::string ref = SoundRefToStr(s.start_sound, common);
    ar(cereal::make_nvp("startSound", ref));
  }
  ar(cereal::make_nvp("numSounds", s.num_sounds));
  ar(cereal::make_nvp("animDelay", s.anim_delay));
  ar(cereal::make_nvp("startFrame", s.start_frame));
  ar(cereal::make_nvp("numFrames", s.num_frames));
  ar(cereal::make_nvp("detectRange", s.detect_range));
  ar(cereal::make_nvp("damage", s.damage));
  ar(cereal::make_nvp("blowAway", s.blow_away));
  ar(cereal::make_nvp("shake", s.shake));
  ar(cereal::make_nvp("flash", s.flash));
  ar(cereal::make_nvp("dirtEffect", s.dirt_effect));
}

inline void LoadSObjectConfig(Common const& common, SObjectType& s, std::istream& is) {
  cereal::TomlInputArchive ar(is);
  ar(cereal::make_nvp("shadow", s.shadow));
  {
    std::string ref;
    ar(cereal::make_nvp("startSound", ref));
    s.start_sound = SoundRefFromStr(ref, common);
  }
  ar(cereal::make_nvp("numSounds", s.num_sounds));
  ar(cereal::make_nvp("animDelay", s.anim_delay));
  ar(cereal::make_nvp("startFrame", s.start_frame));
  ar(cereal::make_nvp("numFrames", s.num_frames));
  ar(cereal::make_nvp("detectRange", s.detect_range));
  ar(cereal::make_nvp("damage", s.damage));
  ar(cereal::make_nvp("blowAway", s.blow_away));
  ar(cereal::make_nvp("shake", s.shake));
  ar(cereal::make_nvp("flash", s.flash));
  ar(cereal::make_nvp("dirtEffect", s.dirt_effect));
}

// Save/load Weapon config
inline void SaveWeaponConfig(Common const& common, Weapon const& w, std::ostream& os) {
  cereal::TomlOutputArchive ar(os);
  ar(cereal::make_nvp("name", w.name));
  ar(cereal::make_nvp("affectByWorm", w.affect_by_worm));
  ar(cereal::make_nvp("shadow", w.shadow));
  ar(cereal::make_nvp("laserSight", w.laser_sight));
  ar(cereal::make_nvp("playReloadSound", w.play_reload_sound));
  ar(cereal::make_nvp("wormExplode", w.worm_explode));
  ar(cereal::make_nvp("explGround", w.expl_ground));
  ar(cereal::make_nvp("wormCollide", w.worm_collide));
  ar(cereal::make_nvp("collideWithObjects", w.collide_with_objects));
  ar(cereal::make_nvp("affectByExplosions", w.affect_by_explosions));
  ar(cereal::make_nvp("loopAnim", w.loop_anim));
  ar(cereal::make_nvp("detectDistance", w.detect_distance));
  ar(cereal::make_nvp("blowAway", w.blow_away));
  ar(cereal::make_nvp("gravity", w.gravity));
  {
    std::string ref = SoundRefToStr(w.launch_sound, common);
    ar(cereal::make_nvp("launchSound", ref));
  }
  {
    std::string ref = SoundRefToStr(w.loop_sound, common);
    ar(cereal::make_nvp("loopSound", ref));
  }
  {
    std::string ref = SoundRefToStr(w.explo_sound, common);
    ar(cereal::make_nvp("exploSound", ref));
  }
  ar(cereal::make_nvp("speed", w.speed));
  ar(cereal::make_nvp("addSpeed", w.add_speed));
  ar(cereal::make_nvp("distribution", w.distribution));
  ar(cereal::make_nvp("parts", w.parts));
  ar(cereal::make_nvp("recoil", w.recoil));
  ar(cereal::make_nvp("multSpeed", w.mult_speed));
  ar(cereal::make_nvp("delay", w.delay));
  ar(cereal::make_nvp("loadingTime", w.loading_time));
  ar(cereal::make_nvp("ammo", w.ammo));
  ar(cereal::make_nvp("dirtEffect", w.dirt_effect));
  ar(cereal::make_nvp("leaveShells", w.leave_shells));
  ar(cereal::make_nvp("leaveShellDelay", w.leave_shell_delay));
  ar(cereal::make_nvp("fireCone", w.fire_cone));
  ar(cereal::make_nvp("bounce", w.bounce));
  ar(cereal::make_nvp("timeToExplo", w.time_to_explo));
  ar(cereal::make_nvp("timeToExploV", w.time_to_explo_v));
  ar(cereal::make_nvp("hitDamage", w.hit_damage));
  ar(cereal::make_nvp("bloodOnHit", w.blood_on_hit));
  ar(cereal::make_nvp("startFrame", w.start_frame));
  ar(cereal::make_nvp("numFrames", w.num_frames));
  ar(cereal::make_nvp("shotType", w.shot_type));
  ar(cereal::make_nvp("colorBullets", w.color_bullets));
  ar(cereal::make_nvp("splinterAmount", w.splinter_amount));
  ar(cereal::make_nvp("splinterColour", w.splinter_colour));
  {
    std::string ref = ObjRefToStr(w.splinter_type, common.nobject_types);
    ar(cereal::make_nvp("splinterType", ref));
  }
  ar(cereal::make_nvp("splinterScatter", w.splinter_scatter));
  {
    std::string ref = ObjRefToStr(w.obj_trail_type, common.sobject_types);
    ar(cereal::make_nvp("objTrailType", ref));
  }
  ar(cereal::make_nvp("objTrailDelay", w.obj_trail_delay));
  ar(cereal::make_nvp("partTrailType", w.part_trail_type));
  {
    std::string ref = ObjRefToStr(w.part_trail_obj, common.nobject_types);
    ar(cereal::make_nvp("partTrailObj", ref));
  }
  ar(cereal::make_nvp("partTrailDelay", w.part_trail_delay));
  {
    std::string ref = ObjRefToStr(w.create_on_exp, common.sobject_types);
    ar(cereal::make_nvp("createOnExp", ref));
  }
  ar(cereal::make_nvp("chainExplosion", w.chain_explosion));
}

inline void LoadWeaponConfig(Common& common, Weapon& w, std::istream& is) {
  cereal::TomlInputArchive ar(is);
  ar(cereal::make_nvp("name", w.name));
  ar(cereal::make_nvp("affectByWorm", w.affect_by_worm));
  ar(cereal::make_nvp("shadow", w.shadow));
  ar(cereal::make_nvp("laserSight", w.laser_sight));
  ar(cereal::make_nvp("playReloadSound", w.play_reload_sound));
  ar(cereal::make_nvp("wormExplode", w.worm_explode));
  ar(cereal::make_nvp("explGround", w.expl_ground));
  ar(cereal::make_nvp("wormCollide", w.worm_collide));
  ar(cereal::make_nvp("collideWithObjects", w.collide_with_objects));
  ar(cereal::make_nvp("affectByExplosions", w.affect_by_explosions));
  ar(cereal::make_nvp("loopAnim", w.loop_anim));
  ar(cereal::make_nvp("detectDistance", w.detect_distance));
  ar(cereal::make_nvp("blowAway", w.blow_away));
  ar(cereal::make_nvp("gravity", w.gravity));
  {
    std::string ref;
    ar(cereal::make_nvp("launchSound", ref));
    w.launch_sound = SoundRefFromStr(ref, common);
  }
  {
    std::string ref;
    ar(cereal::make_nvp("loopSound", ref));
    w.loop_sound = SoundRefFromStr(ref, common);
  }
  {
    std::string ref;
    ar(cereal::make_nvp("exploSound", ref));
    w.explo_sound = SoundRefFromStr(ref, common);
  }
  ar(cereal::make_nvp("speed", w.speed));
  ar(cereal::make_nvp("addSpeed", w.add_speed));
  ar(cereal::make_nvp("distribution", w.distribution));
  ar(cereal::make_nvp("parts", w.parts));
  ar(cereal::make_nvp("recoil", w.recoil));
  ar(cereal::make_nvp("multSpeed", w.mult_speed));
  ar(cereal::make_nvp("delay", w.delay));
  ar(cereal::make_nvp("loadingTime", w.loading_time));
  ar(cereal::make_nvp("ammo", w.ammo));
  ar(cereal::make_nvp("dirtEffect", w.dirt_effect));
  ar(cereal::make_nvp("leaveShells", w.leave_shells));
  ar(cereal::make_nvp("leaveShellDelay", w.leave_shell_delay));
  ar(cereal::make_nvp("fireCone", w.fire_cone));
  ar(cereal::make_nvp("bounce", w.bounce));
  ar(cereal::make_nvp("timeToExplo", w.time_to_explo));
  ar(cereal::make_nvp("timeToExploV", w.time_to_explo_v));
  ar(cereal::make_nvp("hitDamage", w.hit_damage));
  ar(cereal::make_nvp("bloodOnHit", w.blood_on_hit));
  ar(cereal::make_nvp("startFrame", w.start_frame));
  ar(cereal::make_nvp("numFrames", w.num_frames));
  ar(cereal::make_nvp("shotType", w.shot_type));
  ar(cereal::make_nvp("colorBullets", w.color_bullets));
  ar(cereal::make_nvp("splinterAmount", w.splinter_amount));
  ar(cereal::make_nvp("splinterColour", w.splinter_colour));
  {
    std::string ref;
    ar(cereal::make_nvp("splinterType", ref));
    w.splinter_type = ObjRefFromStr(ref, common.nobject_types);
  }
  ar(cereal::make_nvp("splinterScatter", w.splinter_scatter));
  {
    std::string ref;
    ar(cereal::make_nvp("objTrailType", ref));
    w.obj_trail_type = ObjRefFromStr(ref, common.sobject_types);
  }
  ar(cereal::make_nvp("objTrailDelay", w.obj_trail_delay));
  ar(cereal::make_nvp("partTrailType", w.part_trail_type));
  {
    std::string ref;
    ar(cereal::make_nvp("partTrailObj", ref));
    w.part_trail_obj = ObjRefFromStr(ref, common.nobject_types);
  }
  ar(cereal::make_nvp("partTrailDelay", w.part_trail_delay));
  {
    std::string ref;
    ar(cereal::make_nvp("createOnExp", ref));
    w.create_on_exp = ObjRefFromStr(ref, common.sobject_types);
  }
  ar(cereal::make_nvp("chainExplosion", w.chain_explosion));
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
  int32_t timer_v = 0;
  int32_t frame = 0;
  std::string sobj;
  template <typename Archive>
  void serialize(Archive& ar) {
    ar(cereal::make_nvp("timer", timer));
    ar(cereal::make_nvp("timerV", timer_v));
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
  std::vector<ColourAnimEntry> color_anim;
  std::vector<int32_t> materials;
  AiParamsS aiparams;
#define DECL_FIELD_C(n) int32_t n = 0;
  LIERO_CDEFS(DECL_FIELD_C)
#undef DECL_FIELD_C

  template <typename Archive>
  void serialize(Archive& ar) {
    ar(cereal::make_nvp("bonuses", bonuses));
    ar(cereal::make_nvp("textures", textures));
    ar(cereal::make_nvp("colorAnim", color_anim));
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
inline void SaveTcConfig(Common const& common, std::ostream& os) {
  tc_cfg::Types types;
  for (const auto& s : common.sounds) types.sounds.push_back(s.name);
  for (const auto& w : common.weapons) types.weapons.push_back(w.id_str);
  for (const auto& n : common.nobject_types) types.nobjects.push_back(n.id_str);
  for (const auto& s : common.sobject_types) types.sobjects.push_back(s.id_str);

  tc_cfg::Constants constants;
  for (int i = 0; i < NUM_BONUS_SOBJECTS; ++i) {
    tc_cfg::BonusEntry be;
    be.timer = common.bonus_rand_timer[i][0];
    be.timer_v = common.bonus_rand_timer[i][1];
    be.frame = common.bonus_frames[i];
    be.sobj = ObjRefToStr(common.bonus_s_objects[i], common.sobject_types);
    constants.bonuses.push_back(be);
  }
  for (auto texture : common.textures) {
    tc_cfg::TextureEntry te;
    te.mframe = texture.m_frame;
    te.rframe = texture.r_frame;
    te.sframe = texture.s_frame;
    te.ndrawback = texture.n_draw_back;
    constants.textures.push_back(te);
  }
  for (auto i : common.color_anim) {
    tc_cfg::ColourAnimEntry ce;
    ce.from = i.from;
    ce.to = i.to;
    constants.color_anim.push_back(ce);
  }
  for (auto material : common.materials) constants.materials.push_back(material.flags);

  auto fill_ai_key = [&](tc_cfg::AiKey& k, int idx) {
    k.on = common.ai_params.k[1][idx];
    k.off = common.ai_params.k[0][idx];
  };
  fill_ai_key(constants.aiparams.up, 0);
  fill_ai_key(constants.aiparams.down, 1);
  fill_ai_key(constants.aiparams.left, 2);
  fill_ai_key(constants.aiparams.right, 3);
  fill_ai_key(constants.aiparams.fire, 4);
  fill_ai_key(constants.aiparams.change, 5);
  fill_ai_key(constants.aiparams.jump, 6);

#define COPY_FIELD_C(n) constants.n = common.c[C##n];
  LIERO_CDEFS(COPY_FIELD_C)
#undef COPY_FIELD_C

  tc_cfg::TextsS texts;
#define COPY_FIELD_S(n) texts.n = common.s[S##n];
  LIERO_SDEFS(COPY_FIELD_S)
#undef COPY_FIELD_S

  tc_cfg::Hacks hacks;
#define COPY_FIELD_H(n) hacks.n = common.h[H##n];
  LIERO_HDEFS(COPY_FIELD_H)
#undef COPY_FIELD_H

  tc_cfg::Sounds sounds;
#define COPY_FIELD_SO(n)                                                            \
  sounds.n = (common.sound_hook[Sound##n] >= 0 &&                                   \
              common.sound_hook[Sound##n] < static_cast<int>(common.sounds.size())) \
                 ? common.sounds[common.sound_hook[Sound##n]].name                  \
                 : std::string();
  // NOLINTNEXTLINE(modernize-use-integer-sign-comparison) — macro expansion blends int and size_t intentionally.
  LIERO_SOUNDDEFS(COPY_FIELD_SO)
#undef COPY_FIELD_SO

  cereal::TomlOutputArchive ar(os);
  ar(cereal::make_nvp("types", types));
  ar(cereal::make_nvp("constants", constants));
  ar(cereal::make_nvp("texts", texts));
  ar(cereal::make_nvp("hacks", hacks));
  ar(cereal::make_nvp("sounds", sounds));
}

inline void LoadTcConfig(Common& common, std::istream& is) {
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
  for (std::size_t i = 0; i < types.weapons.size(); ++i)
    common.weapons[i].id_str = types.weapons[i];

  common.nobject_types.resize(types.nobjects.size());
  for (std::size_t i = 0; i < types.nobjects.size(); ++i)
    common.nobject_types[i].id_str = types.nobjects[i];

  common.sobject_types.resize(types.sobjects.size());
  for (std::size_t i = 0; i < types.sobjects.size(); ++i)
    common.sobject_types[i].id_str = types.sobjects[i];

  // Constants
  for (std::size_t i = 0; i < constants.bonuses.size() && i < NUM_BONUS_SOBJECTS; ++i) {
    common.bonus_rand_timer[i][0] = constants.bonuses[i].timer;
    common.bonus_rand_timer[i][1] = constants.bonuses[i].timer_v;
    common.bonus_frames[i] = constants.bonuses[i].frame;
    common.bonus_s_objects[i] = ObjRefFromStr(constants.bonuses[i].sobj, common.sobject_types);
  }

  for (std::size_t i = 0; i < constants.textures.size() && i < NUM_TEXTURES; ++i) {
    common.textures[i].m_frame = constants.textures[i].mframe;
    common.textures[i].r_frame = constants.textures[i].rframe;
    common.textures[i].s_frame = constants.textures[i].sframe;
    common.textures[i].n_draw_back = constants.textures[i].ndrawback;
  }

  for (std::size_t i = 0; i < constants.color_anim.size() && i < NUM_COLOR_ANIM; ++i) {
    common.color_anim[i].from = constants.color_anim[i].from;
    common.color_anim[i].to = constants.color_anim[i].to;
  }

  for (std::size_t i = 0; i < constants.materials.size() && i < MAX_MATERIALS; ++i) {
    common.materials[i].flags = static_cast<uint8_t>(constants.materials[i] & 0xff);
  }

  auto read_ai_key = [&](tc_cfg::AiKey const& k, int idx) {
    common.ai_params.k[1][idx] = k.on;
    common.ai_params.k[0][idx] = k.off;
  };
  read_ai_key(constants.aiparams.up, 0);
  read_ai_key(constants.aiparams.down, 1);
  read_ai_key(constants.aiparams.left, 2);
  read_ai_key(constants.aiparams.right, 3);
  read_ai_key(constants.aiparams.fire, 4);
  read_ai_key(constants.aiparams.change, 5);
  read_ai_key(constants.aiparams.jump, 6);

#define COPY_FIELD_C2(n) common.c[C##n] = constants.n;
  LIERO_CDEFS(COPY_FIELD_C2)
#undef COPY_FIELD_C2

#define COPY_FIELD_S2(n) common.s[S##n] = texts.n;
  LIERO_SDEFS(COPY_FIELD_S2)
#undef COPY_FIELD_S2

#define COPY_FIELD_H2(n) common.h[H##n] = hacks.n;
  LIERO_HDEFS(COPY_FIELD_H2)
#undef COPY_FIELD_H2

  // Resolve [sounds] hook names against the now-loaded sound table.
  // Missing/empty entries fall back to the canonical Liero sound name
  // for that hook (so TCs whose tc.cfg predates the [sounds] section
  // still get menu/round/bump/reload sounds). Names that don't match
  // any loaded sound get a warning and resolve to -1.
  auto resolveHook = [&](SoundDefT hook, std::string const& configured, char const* default_name,
                         char const* hook_label) {
    if (!configured.empty()) {
      int const kIdx = common.SoundIndex(configured);
      if (kIdx < 0)
        console::WriteWarning(std::string("[sounds] ") + hook_label +
                              " references unknown sound \"" + configured + "\"");
      common.sound_hook[hook] = kIdx;
    } else {
      common.sound_hook[hook] = common.SoundIndex(default_name);
    }
  };
#define COPY_FIELD_SO2(n) resolveHook(Sound##n, sounds.n, kDefaultSound##n, #n);
  // Canonical Liero sound names per hook. Kept in sync with tctool's
  // extracted sound table; serves as the fallback when [sounds] is
  // missing from tc.cfg.
  constexpr char const* kDefaultSoundMenuMoveUp = "moveup";
  constexpr char const* kDefaultSoundMenuMoveDown = "movedown";
  constexpr char const* kDefaultSoundMenuSelect = "select";
  constexpr char const* kDefaultSoundBump = "bump";
  constexpr char const* kDefaultSoundBegin = "begin";
  constexpr char const* kDefaultSoundReloaded = "reloaded";
  constexpr char const* kDefaultSoundAlive = "alive";
  constexpr char const* kDefaultSoundNinjaropeThrow = "throw";
  LIERO_SOUNDDEFS(COPY_FIELD_SO2)
#undef COPY_FIELD_SO2
}
