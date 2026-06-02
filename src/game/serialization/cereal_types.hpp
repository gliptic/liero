#pragma once

// Cereal serialize() glue for openliero gameplay types.
//
// Each type's existing header stays free of cereal so include-graph cost
// is paid only by code that actually serializes. Include this header from
// replay.cpp / settings.cpp and from cereal-specific test files.
//
// Convention: non-member serialize() templated on Archive so the same
// function works for cereal::PortableBinary{In,Out}putArchive and
// cereal::Toml{In,Out}putArchive.

#include <cereal/cereal.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/string.hpp>

#include "gfx/color.hpp"
#include "gfx/palette.hpp"
#include "level.hpp"
#include "math/rect.hpp"
#include "rand.hpp"
#include "settings.hpp"
#include "viewport.hpp"
#include "worm.hpp"

#include <cereal/types/vector.hpp>
#include <cstdint>

template <class Archive, typename T>
void serialize(Archive& ar, BasicVec<T, 2>& v) {
  ar(cereal::make_nvp("x", v.x), cereal::make_nvp("y", v.y));
}

template <class Archive, typename T>
void serialize(Archive& ar, BasicRect<T>& r) {
  ar(cereal::make_nvp("x1", r.x1), cereal::make_nvp("y1", r.y1), cereal::make_nvp("x2", r.x2),
     cereal::make_nvp("y2", r.y2));
}

// Serialize a C array as a cereal array (produces inline TOML arrays).
template <class Archive, typename T, std::size_t N>
void SerializeArray(Archive& ar, const char* name, T (&arr)[N]) {
  ar.setNextName(name);
  ar.startNode();
  cereal::size_type size = N;
  ar(cereal::make_size_tag(size));
  for (std::size_t i = 0; i < N; ++i) ar(arr[i]);
  ar.finishNode();
}

// ---- Worm::ControlState ----
// Single bitfield; serialized through the underlying uint32_t.
template <class Archive>
void serialize(Archive& ar, Worm::ControlState& cs) {
  ar(cereal::make_nvp("istate", cs.istate));
}

// ---- Ninjarope ----
// `anchor` (Worm*) is intentionally NOT serialized here: pointer dedup and
// worm-graph reconstruction belong to the enclosing Worm/Game scope, which
// will serialize the anchor by index (mirroring lastKilledByIdx from
// Phase 3). Everything else is plain data.
template <class Archive>
void serialize(Archive& ar, Ninjarope& n) {
  ar(cereal::make_nvp("out", n.out), cereal::make_nvp("attached", n.attached),
     cereal::make_nvp("pos", n.pos), cereal::make_nvp("vel", n.vel),
     cereal::make_nvp("length", n.length), cereal::make_nvp("curLen", n.cur_len));
}

// ---- Rand ----
// std::mt19937 is portably serializable via its stream format; we wrap
// it in a save/load pair so cereal can handle it.
template <class Archive>
void save(Archive& ar, Rand const& r) {
  std::string state = r.serialize();
  ar(cereal::make_nvp("state", state), cereal::make_nvp("last", r.last));
}

template <class Archive>
void load(Archive& ar, Rand& r) {
  std::string state;
  ar(cereal::make_nvp("state", state), cereal::make_nvp("last", r.last));
  r.Deserialize(state);
}

// ---- Color ----
template <class Archive>
void serialize(Archive& ar, Color& c) {
  ar(cereal::make_nvp("r", c.r), cereal::make_nvp("g", c.g), cereal::make_nvp("b", c.b));
}

// ---- Palette ----
// Cereal's vector serialization wraps a SizeTag + element loop. Palette
// has a fixed 256-entry C array, so we drive it manually.
template <class Archive>
void serialize(Archive& ar, Palette& p) {
  for (int i = 0; i < 256; ++i) ar(cereal::make_nvp("c" + std::to_string(i), p.entries[i]));
}

// ---- Level ----
// `materials` is re-derived from `data` + Common at load time (matching
// the existing replay behaviour), so we don't serialize it. `oldRandomLevel`
// / `oldLevelFile` / `zeroMaterial` are also not part of the wire format.
template <class Archive>
void serialize(Archive& ar, Level& lvl) {
  ar(cereal::make_nvp("width", lvl.width), cereal::make_nvp("height", lvl.height),
     cereal::make_nvp("data", lvl.data), cereal::make_nvp("origpal", lvl.origpal));
}

// ---- Settings ----
// Includes GameplayExtensions, AppSettings, and the per-worm settings
// (shared_ptr — cereal tracks identity natively in PortableBinaryArchive).
// `hash` is a runtime cache, deliberately excluded.
// v1: initial cereal migration (all original fields).
// v2: added bonusTimeout (default 0 = no timeout).
// v3: added inputDelay.

// Scalar fields only (no wormSettings or weapTable). The weapTable is
// handled separately because the TOML path writes it as an array while
// the binary path uses indexed fields.
template <class Archive>
void SerializeSettingsScalars(Archive& ar, Settings& s) {
  // GameplayExtensions
  ar(cereal::make_nvp("recordReplays", s.record_replays),
     cereal::make_nvp("loadPowerlevelPalette", s.load_powerlevel_palette),
     cereal::make_nvp("aiFrames", s.ai_frames), cereal::make_nvp("aiMutations", s.ai_mutations),
     cereal::make_nvp("aiTraces", s.ai_traces), cereal::make_nvp("aiParallels", s.ai_parallels),
     cereal::make_nvp("zoneTimeout", s.zone_timeout),
     cereal::make_nvp("selectBotWeapons", s.select_bot_weapons),
     cereal::make_nvp("allowViewingSpawnPoint", s.allow_viewing_spawn_point),
     cereal::make_nvp("tc", s.tc));
  // AppSettings
  ar(cereal::make_nvp("fullscreen", s.fullscreen),
     cereal::make_nvp("singleScreenReplay", s.single_screen_replay),
     cereal::make_nvp("spectatorWindow", s.spectator_window),
     cereal::make_nvp("bloodParticleMax", s.blood_particle_max));
  // Settings proper
  ar(cereal::make_nvp("maxBonuses", s.max_bonuses), cereal::make_nvp("blood", s.blood),
     cereal::make_nvp("timeToLose", s.time_to_lose), cereal::make_nvp("flagsToWin", s.flags_to_win),
     cereal::make_nvp("gameMode", s.game_mode), cereal::make_nvp("shadow", s.shadow),
     cereal::make_nvp("loadChange", s.load_change),
     cereal::make_nvp("namesOnBonuses", s.names_on_bonuses),
     cereal::make_nvp("regenerateLevel", s.regenerate_level), cereal::make_nvp("lives", s.lives),
     cereal::make_nvp("loadingTime", s.loading_time),
     cereal::make_nvp("randomLevel", s.random_level), cereal::make_nvp("levelFile", s.level_file),
     cereal::make_nvp("map", s.map), cereal::make_nvp("screenSync", s.screen_sync));
  ar(cereal::make_nvp("bonusTimeout", s.bonus_timeout));
  // v3 fields. Missing on older configs → defaults remain.
  ar(cereal::make_nvp("inputDelay", s.input_delay));
}

// Full Settings fields for binary archives (indexed weapon keys).
template <class Archive>
void SerializeSettingsFields(Archive& ar, Settings& s) {
  SerializeSettingsScalars(ar, s);
  for (int i = 0; i < 40; ++i) ar(cereal::make_nvp("weap" + std::to_string(i), s.weap_table[i]));
}

template <class Archive>
void serialize(Archive& ar, Settings& s, std::uint32_t const kVersion) {
  SerializeSettingsFields(ar, s);
  for (int i = 0; i < Settings::kNumWormSettings; ++i)
    ar(cereal::make_nvp("worm" + std::to_string(i), s.worm_settings[i]));
  (void)kVersion;  // all fields always written now (breaking change)
}
CEREAL_CLASS_VERSION(Settings, 3);

// Gameplay-only subset for hash computation. AppSettings fields are
// deliberately excluded so UI-only changes don't affect the hash.
template <class Archive>
void SerializeGameplay(Archive& ar, Settings& s) {
  ar(cereal::make_nvp("recordReplays", s.record_replays),
     cereal::make_nvp("loadPowerlevelPalette", s.load_powerlevel_palette),
     cereal::make_nvp("aiFrames", s.ai_frames), cereal::make_nvp("aiMutations", s.ai_mutations),
     cereal::make_nvp("aiTraces", s.ai_traces), cereal::make_nvp("aiParallels", s.ai_parallels),
     cereal::make_nvp("zoneTimeout", s.zone_timeout),
     cereal::make_nvp("selectBotWeapons", s.select_bot_weapons),
     cereal::make_nvp("allowViewingSpawnPoint", s.allow_viewing_spawn_point),
     cereal::make_nvp("tc", s.tc));
  ar(cereal::make_nvp("maxBonuses", s.max_bonuses), cereal::make_nvp("blood", s.blood),
     cereal::make_nvp("timeToLose", s.time_to_lose), cereal::make_nvp("flagsToWin", s.flags_to_win),
     cereal::make_nvp("gameMode", s.game_mode), cereal::make_nvp("shadow", s.shadow),
     cereal::make_nvp("loadChange", s.load_change),
     cereal::make_nvp("namesOnBonuses", s.names_on_bonuses),
     cereal::make_nvp("regenerateLevel", s.regenerate_level), cereal::make_nvp("lives", s.lives),
     cereal::make_nvp("loadingTime", s.loading_time),
     cereal::make_nvp("randomLevel", s.random_level), cereal::make_nvp("levelFile", s.level_file),
     cereal::make_nvp("map", s.map), cereal::make_nvp("screenSync", s.screen_sync));
  SerializeArray(ar, "weapTable", s.weap_table);
  ar(cereal::make_nvp("bonusTimeout", s.bonus_timeout));
  ar(cereal::make_nvp("inputDelay", s.input_delay));
}

// ---- Viewport ----
// Pure data; no context dependencies. The `rand` member isn't actually
// consumed anywhere (the old archive just wrote two dummy u32s), so it
// is intentionally omitted here.
template <class Archive>
void serialize(Archive& ar, Viewport& v) {
  ar(cereal::make_nvp("x", v.x), cereal::make_nvp("y", v.y), cereal::make_nvp("shake", v.shake),
     cereal::make_nvp("maxX", v.max_x), cereal::make_nvp("maxY", v.max_y),
     cereal::make_nvp("centerX", v.center_x), cereal::make_nvp("centerY", v.center_y),
     cereal::make_nvp("wormIdx", v.worm_idx), cereal::make_nvp("bannerY", v.banner_y),
     cereal::make_nvp("rect", v.rect));
}

// ---- WormSettings (gameplay subset) ----
// Plain data only; `profileNode` (FsNode) and `hash` are deliberately
// out — they're transient state.
template <class Archive>
void serialize(Archive& ar, WormSettings& ws) {
  ar(cereal::make_nvp("health", ws.health), cereal::make_nvp("controller", ws.controller),
     cereal::make_nvp("name", ws.name), cereal::make_nvp("randomName", ws.random_name),
     cereal::make_nvp("color", ws.color), cereal::make_nvp("rgb0", ws.rgb[0]),
     cereal::make_nvp("rgb1", ws.rgb[1]), cereal::make_nvp("rgb2", ws.rgb[2]),
     cereal::make_nvp("inputDevice", ws.input_device),
     cereal::make_nvp("gamepadName", ws.gamepad_name),
     cereal::make_nvp("gamepadSerial", ws.gamepad_serial));
  for (int i = 0; i < NUM_WEAPONS; ++i)
    ar(cereal::make_nvp("weapon" + std::to_string(i), ws.weapons[i]));
  for (int i = 0; i < WormSettingsExtensions::kMaxControl; ++i)
    ar(cereal::make_nvp("control" + std::to_string(i), ws.controls[i]));
  for (int i = 0; i < WormSettingsExtensions::kMaxControlEx; ++i)
    ar(cereal::make_nvp("controlEx" + std::to_string(i), ws.controls_ex[i]));
  for (int i = 0; i < WormSettingsExtensions::kMaxControlEx; ++i)
    ar(cereal::make_nvp("gpControl" + std::to_string(i), ws.gamepad_controls[i]));
}

// TOML-specific WormSettings serialization: uses arrays instead of indexed keys.
template <class Archive>
void SerializeWormSettingsToml(Archive& ar, WormSettings& ws) {
  ar(cereal::make_nvp("name", ws.name), cereal::make_nvp("health", ws.health),
     cereal::make_nvp("controller", ws.controller), cereal::make_nvp("randomName", ws.random_name),
     cereal::make_nvp("color", ws.color), cereal::make_nvp("inputDevice", ws.input_device),
     cereal::make_nvp("gamepadName", ws.gamepad_name),
     cereal::make_nvp("gamepadSerial", ws.gamepad_serial));
  SerializeArray(ar, "rgb", ws.rgb);
  SerializeArray(ar, "weapons", ws.weapons);
  SerializeArray(ar, "controls", ws.controls);
  SerializeArray(ar, "controlsEx", ws.controls_ex);
  SerializeArray(ar, "gamepadControls", ws.gamepad_controls);
}

// ---- WormWeapon ----
// `type` (Weapon const*) is intentionally NOT serialized here: it's looked
// up by index in Common::weapons at the enclosing scope (same pattern as
// the existing replay archive). Plain numeric fields only.
template <class Archive>
void serialize(Archive& ar, WormWeapon& w) {
  ar(cereal::make_nvp("ammo", w.ammo), cereal::make_nvp("delayLeft", w.delay_left),
     cereal::make_nvp("loadingLeft", w.loading_left));
}

// ---- Worm ----
// Plain gameplay state. Context-dependent fields are intentionally
// excluded:
//   - ninjarope.anchor (Worm*) — Game::serialize() writes/reads the
//     anchor index alongside each worm.
//   - weapons[i].type (Weapon const*) — Game::serialize() writes/reads
//     the index into Common::weapons.
//   - settings (shared_ptr<WormSettings>) — Game::serialize() handles
//     worm settings (they share lifetime with the global settings).
//   - ai (shared_ptr<WormAI>) — transient, rebuilt on load.
template <class Archive>
void serialize(Archive& ar, Worm& w) {
  ar(cereal::make_nvp("pos", w.pos), cereal::make_nvp("vel", w.vel),
     cereal::make_nvp("logicRespawn", w.logic_respawn), cereal::make_nvp("hotspotX", w.hotspot_x),
     cereal::make_nvp("hotspotY", w.hotspot_y), cereal::make_nvp("aimingAngle", w.aiming_angle),
     cereal::make_nvp("aimingSpeed", w.aiming_speed),
     cereal::make_nvp("ableToJump", w.able_to_jump), cereal::make_nvp("ableToDig", w.able_to_dig),
     cereal::make_nvp("keyChangePressed", w.key_change_pressed),
     cereal::make_nvp("movable", w.movable), cereal::make_nvp("animate", w.animate),
     cereal::make_nvp("visible", w.visible), cereal::make_nvp("ready", w.ready),
     cereal::make_nvp("flag", w.flag), cereal::make_nvp("makeSightGreen", w.make_sight_green),
     cereal::make_nvp("health", w.health), cereal::make_nvp("lives", w.lives),
     cereal::make_nvp("kills", w.kills), cereal::make_nvp("timer", w.timer),
     cereal::make_nvp("killedTimer", w.killed_timer),
     cereal::make_nvp("currentFrame", w.current_frame), cereal::make_nvp("flags", w.flags),
     cereal::make_nvp("ninjarope", w.ninjarope),
     cereal::make_nvp("currentWeapon", w.current_weapon),
     cereal::make_nvp("lastKilledByIdx", w.last_killed_by_idx),
     cereal::make_nvp("fireCone", w.fire_cone),
     cereal::make_nvp("leaveShellTimer", w.leave_shell_timer), cereal::make_nvp("index", w.index),
     cereal::make_nvp("direction", w.direction),
     cereal::make_nvp("controlStates", w.control_states),
     cereal::make_nvp("prevControlStates", w.prev_control_states));
  for (int i = 0; i < 4; ++i) ar(cereal::make_nvp("react" + std::to_string(i), w.reacts[i]));
  for (int i = 0; i < NUM_WEAPONS; ++i)
    ar(cereal::make_nvp("weapon" + std::to_string(i), w.weapons[i]));
}

// ---- Game ----
// save/load pair: handles context-dependent fields that span worms.
// The flat per-type serialize() above handles each worm's plain data;
// this Game-level function adds:
//   - ninjarope.anchor → worm index
//   - weapons[i].type → index into Common::weapons
//   - worm settings (shared_ptr, cereal tracks identity)
//   - viewports

#include "game.hpp"

template <class Archive>
void save(Archive& ar, Game const& game) {
  // Settings (shared_ptr — cereal handles identity tracking in binary)
  ar(cereal::make_nvp("settings", game.settings));

  // Scalars
  ar(cereal::make_nvp("cycles", game.cycles), cereal::make_nvp("gotChanged", game.got_changed),
     cereal::make_nvp("lastKilledIdx", game.last_killed_idx),
     cereal::make_nvp("screenFlash", game.screen_flash));

  // Rand
  ar(cereal::make_nvp("rand", const_cast<Rand&>(game.rand)));

  // Worms
  cereal::size_type worm_count = game.worms.size();
  ar(cereal::make_size_tag(worm_count));
  for (auto const& worm_sp : game.worms) {
    Worm const& w = *worm_sp;
    // Flat worm data
    ar(cereal::make_nvp("worm", const_cast<Worm&>(w)));

    // Context: anchor worm index (-1 if null)
    int32_t anchor_idx = -1;
    if (w.ninjarope.anchor) {
      for (std::size_t i = 0; i < game.worms.size(); ++i) {
        if (game.worms[i].get() == w.ninjarope.anchor) {
          anchor_idx = static_cast<int32_t>(i);
          break;
        }
      }
    }
    ar(cereal::make_nvp("anchorIdx", anchor_idx));

    // Context: weapon type indices (-1 if null)
    for (int i = 0; i < NUM_WEAPONS; ++i) {
      int32_t weap_idx = w.weapons[i].type
                             ? static_cast<int32_t>(w.weapons[i].type - &game.common->weapons[0])
                             : -1;
      ar(cereal::make_nvp("weapIdx" + std::to_string(i), weap_idx));
    }

    // Per-worm settings (shared_ptr — cereal tracks sharing)
    ar(cereal::make_nvp("wormSettings", w.settings));
  }

  // Viewports
  cereal::size_type vp_count = game.viewports.size();
  ar(cereal::make_size_tag(vp_count));
  for (auto* vp : game.viewports) {
    ar(cereal::make_nvp("viewport", *vp));
  }

  // Level
  ar(cereal::make_nvp("level", const_cast<Level&>(game.level)));
}

template <class Archive>
void load(Archive& ar, Game& game) {
  // Settings
  ar(cereal::make_nvp("settings", game.settings));

  // Scalars
  ar(cereal::make_nvp("cycles", game.cycles), cereal::make_nvp("gotChanged", game.got_changed),
     cereal::make_nvp("lastKilledIdx", game.last_killed_idx),
     cereal::make_nvp("screenFlash", game.screen_flash));

  // Rand
  ar(cereal::make_nvp("rand", game.rand));

  // Worms
  cereal::size_type worm_count;
  ar(cereal::make_size_tag(worm_count));

  game.ClearWorms();
  std::vector<int32_t> anchor_indices(worm_count);
  for (cereal::size_type i = 0; i < worm_count; ++i) {
    auto worm_sp = std::make_shared<Worm>();
    Worm& w = *worm_sp;

    // Flat worm data
    ar(cereal::make_nvp("worm", w));

    // Context: anchor index (resolve after all worms loaded)
    ar(cereal::make_nvp("anchorIdx", anchor_indices[i]));

    // Context: weapon type indices
    for (int j = 0; j < NUM_WEAPONS; ++j) {
      int32_t weap_idx;
      ar(cereal::make_nvp("weapIdx" + std::to_string(j), weap_idx));
      w.weapons[j].type = weap_idx >= 0 ? &game.common->weapons[weap_idx] : nullptr;
    }

    // Per-worm settings
    ar(cereal::make_nvp("wormSettings", w.settings));

    game.AddWorm(worm_sp);
  }

  // Resolve anchor pointers now that all worms exist
  for (std::size_t i = 0; i < game.worms.size(); ++i) {
    int32_t idx = anchor_indices[i];
    game.worms[i]->ninjarope.anchor = (idx >= 0 && idx < static_cast<int32_t>(game.worms.size()))
                                          ? game.worms[idx].get()
                                          : nullptr;
  }

  // Viewports
  cereal::size_type vp_count;
  ar(cereal::make_size_tag(vp_count));
  game.ClearViewports();
  for (cereal::size_type i = 0; i < vp_count; ++i) {
    Viewport* vp = new Viewport();
    ar(cereal::make_nvp("viewport", *vp));
    game.AddViewport(vp);
  }

  // Level
  ar(cereal::make_nvp("level", game.level));

  // Rebuild materials from data + Common (materials are not serialized —
  // they're derived from level.data and the material table in Common).
  game.level.materials.resize(game.level.width * game.level.height);
  for (std::size_t i = 0; i < game.level.data.size(); ++i)
    game.level.materials[i] = game.common->materials[game.level.data[i]];
}
