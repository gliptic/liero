#pragma once

// Cereal glue for full mid-game Game snapshots. Round-trips the entire
// simulation state — including object pools (bonuses/wobjects/sobjects/
// nobjects/bobjects) and holdazone — which the replay-oriented Game
// save/load in cereal_types.hpp does not cover. Kept separate from the
// replay format so replays stay forward-compatible. Used as the
// correctness oracle for the fast in-memory snapshot path.

#include "cereal_types.hpp"

#include "bobject.hpp"
#include "bonus.hpp"
#include "common.hpp"
#include "exactObjectList.hpp"
#include "fastObjectList.hpp"
#include "game.hpp"
#include "nobject.hpp"
#include "sobject.hpp"
#include "weapon.hpp"
#include "worm.hpp"

#include <cereal/cereal.hpp>
#include <cstdint>

// ---- Holdazone ----
template <class Archive>
void serialize(Archive& ar, Holdazone& h) {
  ar(cereal::make_nvp("rect", h.rect), cereal::make_nvp("holderIdx", h.holder_idx),
     cereal::make_nvp("contenderIdx", h.contender_idx),
     cereal::make_nvp("contenderFrames", h.contender_frames),
     cereal::make_nvp("timeoutLeft", h.timeout_left), cereal::make_nvp("zoneWidth", h.zone_width),
     cereal::make_nvp("zoneHeight", h.zone_height));
}

// ---- Bonus (pure POD-ish) ----
template <class Archive>
void serialize(Archive& ar, Bonus& b) {
  ar(cereal::make_nvp("used", b.used), cereal::make_nvp("x", b.x), cereal::make_nvp("y", b.y),
     cereal::make_nvp("velY", b.vel_y), cereal::make_nvp("frame", b.frame),
     cereal::make_nvp("timer", b.timer), cereal::make_nvp("weapon", b.weapon));
}

// ---- SObject (pure POD) ----
template <class Archive>
void serialize(Archive& ar, SObject& s) {
  ar(cereal::make_nvp("used", s.used), cereal::make_nvp("x", s.x), cereal::make_nvp("y", s.y),
     cereal::make_nvp("id", s.id), cereal::make_nvp("curFrame", s.cur_frame),
     cereal::make_nvp("animDelay", s.anim_delay));
}

// ---- BObject (pure POD) ----
template <class Archive>
void serialize(Archive& ar, BObject& b) {
  ar(cereal::make_nvp("pos", b.pos), cereal::make_nvp("vel", b.vel),
     cereal::make_nvp("color", b.color));
}

// NObject / WObject contain pointers (type, firedBy) that we resolve at the
// pool level so the per-object serializer can stay context-free for the
// numeric fields. The Game-level pool save/load below writes the indices.

template <class Archive>
void SerializeNObjectScalars(Archive& ar, NObject& n) {
  ar(cereal::make_nvp("used", n.used), cereal::make_nvp("pos", n.pos),
     cereal::make_nvp("vel", n.vel), cereal::make_nvp("timeLeft", n.time_left),
     cereal::make_nvp("ownerIdx", n.owner_idx), cereal::make_nvp("curFrame", n.cur_frame),
     cereal::make_nvp("hasHit", n.has_hit));
}

template <class Archive>
void SerializeWObjectScalars(Archive& ar, WObject& w) {
  ar(cereal::make_nvp("used", w.used), cereal::make_nvp("pos", w.pos),
     cereal::make_nvp("vel", w.vel), cereal::make_nvp("ownerIdx", w.owner_idx),
     cereal::make_nvp("curFrame", w.cur_frame), cereal::make_nvp("timeLeft", w.time_left),
     cereal::make_nvp("hasHit", w.has_hit));
}

// ---- (wormIdx, slot) encoding for WormWeapon* firedBy ----
struct FiredByRef {
  int8_t worm_idx;
  int8_t slot;
};

inline FiredByRef EncodeFiredBy(Game const& game, WormWeapon const* fb) {
  if (!fb) {
    return {.worm_idx = -1, .slot = -1};
  }
  for (std::size_t wi = 0; wi < game.worms.size(); ++wi) {
    WormWeapon const* base = game.worms[wi]->weapons;
    if (fb >= base && fb < base + NUM_WEAPONS) {
      return {.worm_idx = static_cast<int8_t>(wi), .slot = static_cast<int8_t>(fb - base)};
    }
  }
  return {.worm_idx = -1, .slot = -1};
}

inline WormWeapon* DecodeFiredBy(Game& game, FiredByRef ref) {
  if (ref.worm_idx < 0 || ref.slot < 0) {
    return nullptr;
  }
  if (static_cast<std::size_t>(ref.worm_idx) >= game.worms.size()) {
    return nullptr;
  }
  return &game.worms[ref.worm_idx]->weapons[ref.slot];
}

template <class Archive>
void serialize(Archive& ar, FiredByRef& r) {
  ar(cereal::make_nvp("w", r.worm_idx), cereal::make_nvp("s", r.slot));
}

// ---- ExactObjectList<T, Limit> ----
// Save: per-slot { used, [fields if used] }. Free-list is rebuilt on load
// from the used flags so slot identity (and therefore future allocation
// order) is preserved exactly.
template <class Archive, typename T, int Limit>
void save(Archive& ar, ExactObjectList<T, Limit> const& list) {
  for (int i = 0; i < Limit; ++i) {
    T const& el = list.arr[i];
    ar(cereal::make_nvp("u", el.used));
    if (el.used) {
      ar(cereal::make_nvp("e", const_cast<T&>(el)));
    }
  }
}

template <class Archive, typename T, int Limit>
void load(Archive& ar, ExactObjectList<T, Limit>& list) {
  list.Clear();
  for (int i = 0; i < Limit; ++i) {
    bool used = false;
    ar(cereal::make_nvp("u", used));
    if (used) {
      // Bypass getFreeObject — it picks the lowest free slot, not slot i.
      ar(cereal::make_nvp("e", list.arr[i]));
      list.arr[i].used = true;
      list.free_list[static_cast<uint32_t>(i) >> 5] &=
          ~(static_cast<uint32_t>(1) << (static_cast<uint32_t>(i) & 31));
      ++list.count;
    }
  }
}

// ---- FastObjectList<T> ----
template <class Archive, typename T>
void save(Archive& ar, FastObjectList<T> const& list) {
  auto count = static_cast<uint32_t>(list.count);
  ar(cereal::make_nvp("count", count));
  for (uint32_t i = 0; i < count; ++i) {
    ar(cereal::make_nvp("e", const_cast<T&>(list.arr[i])));
  }
}

template <class Archive, typename T>
void load(Archive& ar, FastObjectList<T>& list) {
  uint32_t count = 0;
  ar(cereal::make_nvp("count", count));
  list.Clear();
  for (uint32_t i = 0; i < count; ++i) {
    T* slot = list.NewObject();
    if (!slot) {
      // List capacity smaller than snapshotted count — bug, but read into
      // a scratch element to keep the stream consumed.
      T scratch{};
      ar(cereal::make_nvp("e", scratch));
    } else {
      ar(cereal::make_nvp("e", *slot));
    }
  }
}

// ---- Snapshot save/load for Game ----
//
// Composes the existing Game save/load (cereal_types.hpp) with the extra
// state that replays don't need: object pools, holdazone, and the type/
// firedBy index sidecars for the typed pools.
template <class Archive>
void SaveGameSnapshot(Archive& ar, Game const& game) {
  save(ar, game);  // existing Game save (settings, worms, viewports, level, ...)

  ar(cereal::make_nvp("holdazone", const_cast<Holdazone&>(game.holdazone)));

  // Bonuses & SObjects: no pointer sidecars needed.
  ar(cereal::make_nvp("bonuses", const_cast<Game::BonusList&>(game.bonuses)));
  ar(cereal::make_nvp("sobjects", const_cast<Game::SObjectList&>(game.sobjects)));

  // NObjects: store scalar fields per slot, with type index + firedBy ref.
  {
    auto const& list = game.nobjects;
    for (int i = 0; i < 600; ++i) {
      NObject const& n = list.arr[i];
      ar(cereal::make_nvp("u", n.used));
      if (n.used) {
        SerializeNObjectScalars(ar, const_cast<NObject&>(n));
        int32_t type_idx =
            n.type ? static_cast<int32_t>(n.type - game.common->nobject_types.data()) : -1;
        FiredByRef fb = EncodeFiredBy(game, n.fired_by);
        ar(cereal::make_nvp("typeIdx", type_idx), cereal::make_nvp("firedBy", fb));
      }
    }
  }

  // WObjects: same shape.
  {
    auto const& list = game.wobjects;
    for (int i = 0; i < 600; ++i) {
      WObject const& w = list.arr[i];
      ar(cereal::make_nvp("u", w.used));
      if (w.used) {
        SerializeWObjectScalars(ar, const_cast<WObject&>(w));
        int32_t type_idx = w.type ? static_cast<int32_t>(w.type - game.common->weapons.data()) : -1;
        FiredByRef fb = EncodeFiredBy(game, w.fired_by);
        ar(cereal::make_nvp("typeIdx", type_idx), cereal::make_nvp("firedBy", fb));
      }
    }
  }

  // Blood particles.
  ar(cereal::make_nvp("bobjects", const_cast<Game::BObjectList&>(game.bobjects)));
}

template <class Archive>
void LoadGameSnapshot(Archive& ar, Game& game) {
  load(ar, game);  // existing Game load — recreates worms, viewports, level

  ar(cereal::make_nvp("holdazone", game.holdazone));

  ar(cereal::make_nvp("bonuses", game.bonuses));
  ar(cereal::make_nvp("sobjects", game.sobjects));

  // NObjects
  {
    auto& list = game.nobjects;
    list.Clear();
    for (int i = 0; i < 600; ++i) {
      bool used = false;
      ar(cereal::make_nvp("u", used));
      if (used) {
        NObject& n = list.arr[i];
        SerializeNObjectScalars(ar, n);
        n.used = true;
        list.free_list[static_cast<uint32_t>(i) >> 5] &=
            ~(static_cast<uint32_t>(1) << (static_cast<uint32_t>(i) & 31));
        ++list.count;
        int32_t type_idx = 0;
        FiredByRef fb;
        ar(cereal::make_nvp("typeIdx", type_idx), cereal::make_nvp("firedBy", fb));
        n.type = (type_idx >= 0) ? &game.common->nobject_types[type_idx] : nullptr;
        n.fired_by = DecodeFiredBy(game, fb);
      }
    }
  }

  // WObjects
  {
    auto& list = game.wobjects;
    list.Clear();
    for (int i = 0; i < 600; ++i) {
      bool used = false;
      ar(cereal::make_nvp("u", used));
      if (used) {
        WObject& w = list.arr[i];
        SerializeWObjectScalars(ar, w);
        w.used = true;
        list.free_list[static_cast<uint32_t>(i) >> 5] &=
            ~(static_cast<uint32_t>(1) << (static_cast<uint32_t>(i) & 31));
        ++list.count;
        int32_t type_idx = 0;
        FiredByRef fb;
        ar(cereal::make_nvp("typeIdx", type_idx), cereal::make_nvp("firedBy", fb));
        w.type = (type_idx >= 0) ? &game.common->weapons[type_idx] : nullptr;
        w.fired_by = DecodeFiredBy(game, fb);
      }
    }
  }

  ar(cereal::make_nvp("bobjects", game.bobjects));
}
