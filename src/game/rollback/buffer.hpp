#pragma once

// Input + snapshot ring buffer.
//
// Holds the last (kMaxRollback + 1) frames of sim state and the input
// bytes that produced them. Slots are pre-allocated; writing a new
// frame reuses the slot at `frame % kCapacity`, evicting whatever was
// there before. The rollback controller reads from `find()` to restore
// a snapshot and calls `write()` after each advanced frame.
//
// Pure data structure — knows nothing about Game, processFrame, or the
// wire format.

#include "serialization/fast_snapshot.hpp"
#include "serialization/weapsel_snapshot.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>

namespace rollback {

// Frames we can roll back through. Derived from the budget in
// docs/ideas/rollback.md ("Buffer Sizing & Player Count"): ~100 ms of
// tolerance at 70 fps.
constexpr int kMaxRollback = 7;

enum class RemoteState : uint8_t {
  Predicted,
  Confirmed,
};

struct Slot {
  // Frame number stored here, or -1 if the slot is empty.
  int frame = -1;
  GameSnapshot snapshot;
  // Weapon-select rollback uses `wsSnap` instead of `snapshot`. Only one
  // of the two is meaningful per frame (a slot is either pre-game-start
  // weapon-select state or in-game sim state). `wsSnap.valid` indicates
  // which.
  WeaponSelectSnap wsSnap;
  uint8_t localInput = 0;
  uint8_t remoteInput = 0;
  RemoteState remoteState = RemoteState::Predicted;
  // Post-frame checksum cached when the snapshot was written. The
  // controller sends it to the desync detector only once the frame is
  // confirmed (forward with real input, on promote of a matching
  // prediction, or at the end of a resim frame that consumed real input).
  uint32_t checksum = 0;
};

class RollbackBuffer {
 public:
  // One extra slot so we can hold frames [F - kMaxRollback, F] inclusive.
  static constexpr std::size_t kCapacity = static_cast<std::size_t>(kMaxRollback) + 1;

  // Pre-size every slot's snapshot vectors. Call once after the level is
  // generated; no allocations happen on subsequent save/load.
  void prepare(Game const& game) {
    for (auto& slot : slots_) slot.snapshot.prepare(game);
  }

  // Reset all slots to empty. Snapshot capacity (sized by prepare) is kept.
  void clear() {
    for (auto& slot : slots_) {
      slot.frame = -1;
      slot.localInput = 0;
      slot.remoteInput = 0;
      slot.remoteState = RemoteState::Predicted;
      slot.checksum = 0;
      slot.wsSnap.valid = false;
    }
    newest_ = -1;
  }

  // Reserve / return the slot keyed to `frame`. If the slot at that ring
  // index already holds `frame`, returns it unchanged so callers can update
  // input/snapshot in place. Otherwise the slot is repurposed for `frame`
  // (its old contents are gone) and inputs reset to defaults.
  //
  // The snapshot field is left untouched here — the caller is expected to
  // overwrite it via Game::saveSnapshotFast when appropriate. This keeps
  // pure input-only updates (e.g. arriving remote input for a future frame)
  // cheap.
  Slot& write(int frame) {
    // indexOf masks via unsigned, so a negative frame would land on a
    // valid slot and corrupt the ring (frame stored as -1, newest_ not
    // updated). Callers must pass a non-negative sim frame.
    assert(frame >= 0);
    Slot& slot = slots_[indexOf(frame)];
    if (slot.frame != frame) {
      slot.frame = frame;
      slot.localInput = 0;
      slot.remoteInput = 0;
      slot.remoteState = RemoteState::Predicted;
      slot.checksum = 0;
      slot.wsSnap.valid = false;
    }
    if (frame > newest_) newest_ = frame;
    return slot;
  }

  Slot* find(int frame) {
    if (!resident(frame)) return nullptr;
    Slot& slot = slots_[indexOf(frame)];
    return slot.frame == frame ? &slot : nullptr;
  }

  Slot const* find(int frame) const {
    if (!resident(frame)) return nullptr;
    Slot const& slot = slots_[indexOf(frame)];
    return slot.frame == frame ? &slot : nullptr;
  }

  // Newest frame written so far, or -1 if empty.
  int newestFrame() const { return newest_; }

  // Oldest frame still resident, or -1 if empty. This is what the rollback
  // controller compares against when deciding whether a confirmed input is
  // too old to apply (which would mean the local peer already advanced past
  // the rollback horizon — a stall condition).
  int oldestFrame() const {
    if (newest_ < 0) return -1;
    int floor = newest_ - static_cast<int>(kCapacity) + 1;
    return floor < 0 ? 0 : floor;
  }

  bool empty() const { return newest_ < 0; }

  std::size_t size() const {
    if (newest_ < 0) return 0;
    int span = newest_ + 1;
    return span < static_cast<int>(kCapacity) ? static_cast<std::size_t>(span) : kCapacity;
  }

 private:
  static std::size_t indexOf(int frame) {
    // frame is non-negative in practice (sim frames start at 0), but stay
    // defensive in case a caller passes -1.
    return static_cast<std::size_t>(static_cast<unsigned>(frame) % kCapacity);
  }

  bool resident(int frame) const {
    return frame >= 0 && frame >= oldestFrame() && frame <= newest_;
  }

  std::array<Slot, kCapacity> slots_{};
  int newest_ = -1;
};

}  // namespace rollback
