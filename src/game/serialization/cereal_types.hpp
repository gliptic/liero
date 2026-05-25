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

#include "math/rect.hpp"
#include "rand.hpp"
#include "settings.hpp"
#include "viewport.hpp"
#include "worm.hpp"
#include "gfx/palette.hpp"
#include "gfx/color.hpp"
#include "level.hpp"

#include <cereal/types/vector.hpp>
#include <cstdint>

template <class Archive, typename T>
void serialize(Archive& ar, BasicVec<T, 2>& v) {
  ar(cereal::make_nvp("x", v.x), cereal::make_nvp("y", v.y));
}

template <class Archive, typename T>
void serialize(Archive& ar, BasicRect<T>& r) {
  ar(cereal::make_nvp("x1", r.x1), cereal::make_nvp("y1", r.y1),
     cereal::make_nvp("x2", r.x2), cereal::make_nvp("y2", r.y2));
}

// Serialize a C array as a cereal array (produces inline TOML arrays).
template <class Archive, typename T, std::size_t N>
void serializeArray(Archive& ar, const char* name, T (&arr)[N]) {
  ar.setNextName(name);
  ar.startNode();
  cereal::size_type size = N;
  ar(cereal::make_size_tag(size));
  for (std::size_t i = 0; i < N; ++i)
    ar(arr[i]);
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
  ar(cereal::make_nvp("out", n.out),
     cereal::make_nvp("attached", n.attached),
     cereal::make_nvp("pos", n.pos),
     cereal::make_nvp("vel", n.vel),
     cereal::make_nvp("length", n.length),
     cereal::make_nvp("curLen", n.curLen));
}

// ---- Rand ----
// std::mt19937 is portably serializable via its stream format; we wrap
// it in a save/load pair so cereal can handle it.
template <class Archive>
void save(Archive& ar, Rand const& r) {
  std::string state = r.serialize();
  ar(cereal::make_nvp("state", state),
     cereal::make_nvp("last", r.last));
}

template <class Archive>
void load(Archive& ar, Rand& r) {
  std::string state;
  ar(cereal::make_nvp("state", state),
     cereal::make_nvp("last", r.last));
  r.deserialize(state);
}

// ---- Color ----
template <class Archive>
void serialize(Archive& ar, Color& c) {
  ar(cereal::make_nvp("r", c.r),
     cereal::make_nvp("g", c.g),
     cereal::make_nvp("b", c.b));
}

// ---- Palette ----
// Cereal's vector serialization wraps a SizeTag + element loop. Palette
// has a fixed 256-entry C array, so we drive it manually.
template <class Archive>
void serialize(Archive& ar, Palette& p) {
  for (int i = 0; i < 256; ++i)
    ar(cereal::make_nvp("c" + std::to_string(i), p.entries[i]));
}

// ---- Level ----
// `materials` is re-derived from `data` + Common at load time (matching
// the existing replay behaviour), so we don't serialize it. `oldRandomLevel`
// / `oldLevelFile` / `zeroMaterial` are also not part of the wire format.
template <class Archive>
void serialize(Archive& ar, Level& lvl) {
  ar(cereal::make_nvp("width", lvl.width),
     cereal::make_nvp("height", lvl.height),
     cereal::make_nvp("data", lvl.data),
     cereal::make_nvp("origpal", lvl.origpal));
}

// ---- Settings ----
// Includes GameplayExtensions, AppSettings, and the per-worm settings
// (shared_ptr — cereal tracks identity natively in PortableBinaryArchive).
// `hash` is a runtime cache, deliberately excluded.
// v1: initial cereal migration (all original fields).
// v2: added bonusTimeout (default 0 = no timeout).

// Scalar fields only (no wormSettings or weapTable). The weapTable is
// handled separately because the TOML path writes it as an array while
// the binary path uses indexed fields.
template <class Archive>
void serializeSettingsScalars(Archive& ar, Settings& s) {
  // GameplayExtensions
  ar(cereal::make_nvp("recordReplays", s.recordReplays),
     cereal::make_nvp("loadPowerlevelPalette", s.loadPowerlevelPalette),
     cereal::make_nvp("aiFrames", s.aiFrames),
     cereal::make_nvp("aiMutations", s.aiMutations),
     cereal::make_nvp("aiTraces", s.aiTraces),
     cereal::make_nvp("aiParallels", s.aiParallels),
     cereal::make_nvp("zoneTimeout", s.zoneTimeout),
     cereal::make_nvp("selectBotWeapons", s.selectBotWeapons),
     cereal::make_nvp("allowViewingSpawnPoint", s.allowViewingSpawnPoint),
     cereal::make_nvp("tc", s.tc));
  // AppSettings
  ar(cereal::make_nvp("fullscreen", s.fullscreen),
     cereal::make_nvp("singleScreenReplay", s.singleScreenReplay),
     cereal::make_nvp("spectatorWindow", s.spectatorWindow),
     cereal::make_nvp("bloodParticleMax", s.bloodParticleMax));
  // Settings proper
  ar(cereal::make_nvp("maxBonuses", s.maxBonuses),
     cereal::make_nvp("blood", s.blood),
     cereal::make_nvp("timeToLose", s.timeToLose),
     cereal::make_nvp("flagsToWin", s.flagsToWin),
     cereal::make_nvp("gameMode", s.gameMode),
     cereal::make_nvp("shadow", s.shadow),
     cereal::make_nvp("loadChange", s.loadChange),
     cereal::make_nvp("namesOnBonuses", s.namesOnBonuses),
     cereal::make_nvp("regenerateLevel", s.regenerateLevel),
     cereal::make_nvp("lives", s.lives),
     cereal::make_nvp("loadingTime", s.loadingTime),
     cereal::make_nvp("randomLevel", s.randomLevel),
     cereal::make_nvp("levelFile", s.levelFile),
     cereal::make_nvp("map", s.map),
     cereal::make_nvp("screenSync", s.screenSync));
  ar(cereal::make_nvp("bonusTimeout", s.bonusTimeout));
}

// Full Settings fields for binary archives (indexed weapon keys).
template <class Archive>
void serializeSettingsFields(Archive& ar, Settings& s) {
  serializeSettingsScalars(ar, s);
  for (int i = 0; i < 40; ++i)
    ar(cereal::make_nvp("weap" + std::to_string(i), s.weapTable[i]));
}

template <class Archive>
void serialize(Archive& ar, Settings& s, std::uint32_t const version) {
  serializeSettingsFields(ar, s);
  for (int i = 0; i < Settings::NumWormSettings; ++i)
    ar(cereal::make_nvp("worm" + std::to_string(i), s.wormSettings[i]));
  (void)version;  // all fields always written now (breaking change)
}
CEREAL_CLASS_VERSION(Settings, 2);

// Gameplay-only subset for hash computation. AppSettings fields are
// deliberately excluded so UI-only changes don't affect the hash.
template <class Archive>
void serializeGameplay(Archive& ar, Settings& s) {
  ar(cereal::make_nvp("recordReplays", s.recordReplays),
     cereal::make_nvp("loadPowerlevelPalette", s.loadPowerlevelPalette),
     cereal::make_nvp("aiFrames", s.aiFrames),
     cereal::make_nvp("aiMutations", s.aiMutations),
     cereal::make_nvp("aiTraces", s.aiTraces),
     cereal::make_nvp("aiParallels", s.aiParallels),
     cereal::make_nvp("zoneTimeout", s.zoneTimeout),
     cereal::make_nvp("selectBotWeapons", s.selectBotWeapons),
     cereal::make_nvp("allowViewingSpawnPoint", s.allowViewingSpawnPoint),
     cereal::make_nvp("tc", s.tc));
  ar(cereal::make_nvp("maxBonuses", s.maxBonuses),
     cereal::make_nvp("blood", s.blood),
     cereal::make_nvp("timeToLose", s.timeToLose),
     cereal::make_nvp("flagsToWin", s.flagsToWin),
     cereal::make_nvp("gameMode", s.gameMode),
     cereal::make_nvp("shadow", s.shadow),
     cereal::make_nvp("loadChange", s.loadChange),
     cereal::make_nvp("namesOnBonuses", s.namesOnBonuses),
     cereal::make_nvp("regenerateLevel", s.regenerateLevel),
     cereal::make_nvp("lives", s.lives),
     cereal::make_nvp("loadingTime", s.loadingTime),
     cereal::make_nvp("randomLevel", s.randomLevel),
     cereal::make_nvp("levelFile", s.levelFile),
     cereal::make_nvp("map", s.map),
     cereal::make_nvp("screenSync", s.screenSync));
  serializeArray(ar, "weapTable", s.weapTable);
  ar(cereal::make_nvp("bonusTimeout", s.bonusTimeout));
}

// ---- Viewport ----
// Pure data; no context dependencies. The `rand` member isn't actually
// consumed anywhere (the old archive just wrote two dummy u32s), so it
// is intentionally omitted here.
template <class Archive>
void serialize(Archive& ar, Viewport& v) {
  ar(cereal::make_nvp("x", v.x),
     cereal::make_nvp("y", v.y),
     cereal::make_nvp("shake", v.shake),
     cereal::make_nvp("maxX", v.maxX),
     cereal::make_nvp("maxY", v.maxY),
     cereal::make_nvp("centerX", v.centerX),
     cereal::make_nvp("centerY", v.centerY),
     cereal::make_nvp("wormIdx", v.wormIdx),
     cereal::make_nvp("bannerY", v.bannerY),
     cereal::make_nvp("rect", v.rect));
}

// ---- WormSettings (gameplay subset) ----
// Plain data only; `profileNode` (FsNode) and `hash` are deliberately
// out — they're transient state.
template <class Archive>
void serialize(Archive& ar, WormSettings& ws) {
  ar(cereal::make_nvp("health", ws.health),
     cereal::make_nvp("controller", ws.controller),
     cereal::make_nvp("name", ws.name),
     cereal::make_nvp("randomName", ws.randomName),
     cereal::make_nvp("color", ws.color),
     cereal::make_nvp("rgb0", ws.rgb[0]),
     cereal::make_nvp("rgb1", ws.rgb[1]),
     cereal::make_nvp("rgb2", ws.rgb[2]),
     cereal::make_nvp("inputDevice", ws.inputDevice),
     cereal::make_nvp("gamepadName", ws.gamepadName),
     cereal::make_nvp("gamepadSerial", ws.gamepadSerial));
  for (int i = 0; i < NUM_WEAPONS; ++i)
    ar(cereal::make_nvp("weapon" + std::to_string(i), ws.weapons[i]));
  for (int i = 0; i < WormSettingsExtensions::MaxControl; ++i)
    ar(cereal::make_nvp("control" + std::to_string(i), ws.controls[i]));
  for (int i = 0; i < WormSettingsExtensions::MaxControlEx; ++i)
    ar(cereal::make_nvp("controlEx" + std::to_string(i), ws.controlsEx[i]));
  for (int i = 0; i < WormSettingsExtensions::MaxControlEx; ++i)
    ar(cereal::make_nvp("gpControl" + std::to_string(i),
                        ws.gamepadControls[i]));
}

// TOML-specific WormSettings serialization: uses arrays instead of indexed keys.
template <class Archive>
void serializeWormSettingsToml(Archive& ar, WormSettings& ws) {
  ar(cereal::make_nvp("name", ws.name),
     cereal::make_nvp("health", ws.health),
     cereal::make_nvp("controller", ws.controller),
     cereal::make_nvp("randomName", ws.randomName),
     cereal::make_nvp("color", ws.color),
     cereal::make_nvp("inputDevice", ws.inputDevice),
     cereal::make_nvp("gamepadName", ws.gamepadName),
     cereal::make_nvp("gamepadSerial", ws.gamepadSerial));
  serializeArray(ar, "rgb", ws.rgb);
  serializeArray(ar, "weapons", ws.weapons);
  serializeArray(ar, "controls", ws.controls);
  serializeArray(ar, "controlsEx", ws.controlsEx);
  serializeArray(ar, "gamepadControls", ws.gamepadControls);
}

// ---- WormWeapon ----
// `type` (Weapon const*) is intentionally NOT serialized here: it's looked
// up by index in Common::weapons at the enclosing scope (same pattern as
// the existing replay archive). Plain numeric fields only.
template <class Archive>
void serialize(Archive& ar, WormWeapon& w) {
  ar(cereal::make_nvp("ammo", w.ammo),
     cereal::make_nvp("delayLeft", w.delayLeft),
     cereal::make_nvp("loadingLeft", w.loadingLeft));
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
  ar(cereal::make_nvp("pos", w.pos),
     cereal::make_nvp("vel", w.vel),
     cereal::make_nvp("logicRespawn", w.logicRespawn),
     cereal::make_nvp("hotspotX", w.hotspotX),
     cereal::make_nvp("hotspotY", w.hotspotY),
     cereal::make_nvp("aimingAngle", w.aimingAngle),
     cereal::make_nvp("aimingSpeed", w.aimingSpeed),
     cereal::make_nvp("ableToJump", w.ableToJump),
     cereal::make_nvp("ableToDig", w.ableToDig),
     cereal::make_nvp("keyChangePressed", w.keyChangePressed),
     cereal::make_nvp("movable", w.movable),
     cereal::make_nvp("animate", w.animate),
     cereal::make_nvp("visible", w.visible),
     cereal::make_nvp("ready", w.ready),
     cereal::make_nvp("flag", w.flag),
     cereal::make_nvp("makeSightGreen", w.makeSightGreen),
     cereal::make_nvp("health", w.health),
     cereal::make_nvp("lives", w.lives),
     cereal::make_nvp("kills", w.kills),
     cereal::make_nvp("timer", w.timer),
     cereal::make_nvp("killedTimer", w.killedTimer),
     cereal::make_nvp("currentFrame", w.currentFrame),
     cereal::make_nvp("flags", w.flags),
     cereal::make_nvp("ninjarope", w.ninjarope),
     cereal::make_nvp("currentWeapon", w.currentWeapon),
     cereal::make_nvp("lastKilledByIdx", w.lastKilledByIdx),
     cereal::make_nvp("fireCone", w.fireCone),
     cereal::make_nvp("leaveShellTimer", w.leaveShellTimer),
     cereal::make_nvp("index", w.index),
     cereal::make_nvp("direction", w.direction),
     cereal::make_nvp("controlStates", w.controlStates),
     cereal::make_nvp("prevControlStates", w.prevControlStates));
  for (int i = 0; i < 4; ++i)
    ar(cereal::make_nvp("react" + std::to_string(i), w.reacts[i]));
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
  ar(cereal::make_nvp("cycles", game.cycles),
     cereal::make_nvp("gotChanged", game.gotChanged),
     cereal::make_nvp("lastKilledIdx", game.lastKilledIdx),
     cereal::make_nvp("screenFlash", game.screenFlash));

  // Rand
  ar(cereal::make_nvp("rand", const_cast<Rand&>(game.rand)));

  // Worms
  cereal::size_type wormCount = game.worms.size();
  ar(cereal::make_size_tag(wormCount));
  for (auto const& worm_sp : game.worms) {
    Worm const& w = *worm_sp;
    // Flat worm data
    ar(cereal::make_nvp("worm", const_cast<Worm&>(w)));

    // Context: anchor worm index (-1 if null)
    int32_t anchorIdx = -1;
    if (w.ninjarope.anchor) {
      for (std::size_t i = 0; i < game.worms.size(); ++i) {
        if (game.worms[i].get() == w.ninjarope.anchor) {
          anchorIdx = static_cast<int32_t>(i);
          break;
        }
      }
    }
    ar(cereal::make_nvp("anchorIdx", anchorIdx));

    // Context: weapon type indices (-1 if null)
    for (int i = 0; i < NUM_WEAPONS; ++i) {
      int32_t weapIdx = w.weapons[i].type
          ? static_cast<int32_t>(w.weapons[i].type - &game.common->weapons[0])
          : -1;
      ar(cereal::make_nvp("weapIdx" + std::to_string(i), weapIdx));
    }

    // Per-worm settings (shared_ptr — cereal tracks sharing)
    ar(cereal::make_nvp("wormSettings", w.settings));
  }

  // Viewports
  cereal::size_type vpCount = game.viewports.size();
  ar(cereal::make_size_tag(vpCount));
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
  ar(cereal::make_nvp("cycles", game.cycles),
     cereal::make_nvp("gotChanged", game.gotChanged),
     cereal::make_nvp("lastKilledIdx", game.lastKilledIdx),
     cereal::make_nvp("screenFlash", game.screenFlash));

  // Rand
  ar(cereal::make_nvp("rand", game.rand));

  // Worms
  cereal::size_type wormCount;
  ar(cereal::make_size_tag(wormCount));

  game.clearWorms();
  std::vector<int32_t> anchorIndices(wormCount);
  for (cereal::size_type i = 0; i < wormCount; ++i) {
    auto worm_sp = std::make_shared<Worm>();
    Worm& w = *worm_sp;

    // Flat worm data
    ar(cereal::make_nvp("worm", w));

    // Context: anchor index (resolve after all worms loaded)
    ar(cereal::make_nvp("anchorIdx", anchorIndices[i]));

    // Context: weapon type indices
    for (int j = 0; j < NUM_WEAPONS; ++j) {
      int32_t weapIdx;
      ar(cereal::make_nvp("weapIdx" + std::to_string(j), weapIdx));
      w.weapons[j].type = weapIdx >= 0
          ? &game.common->weapons[weapIdx]
          : nullptr;
    }

    // Per-worm settings
    ar(cereal::make_nvp("wormSettings", w.settings));

    game.addWorm(worm_sp);
  }

  // Resolve anchor pointers now that all worms exist
  for (std::size_t i = 0; i < game.worms.size(); ++i) {
    int32_t idx = anchorIndices[i];
    game.worms[i]->ninjarope.anchor =
        (idx >= 0 && idx < static_cast<int32_t>(game.worms.size()))
            ? game.worms[idx].get()
            : nullptr;
  }

  // Viewports
  cereal::size_type vpCount;
  ar(cereal::make_size_tag(vpCount));
  game.clearViewports();
  for (cereal::size_type i = 0; i < vpCount; ++i) {
    Viewport* vp = new Viewport();
    ar(cereal::make_nvp("viewport", *vp));
    game.addViewport(vp);
  }

  // Level
  ar(cereal::make_nvp("level", game.level));

  // Rebuild materials from data + Common (materials are not serialized —
  // they're derived from level.data and the material table in Common).
  game.level.materials.resize(game.level.width * game.level.height);
  for (std::size_t i = 0; i < game.level.data.size(); ++i)
    game.level.materials[i] = game.common->materials[game.level.data[i]];
}
