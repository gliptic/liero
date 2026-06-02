// Input + snapshot ring buffer.
//
// Pure data-structure test: nothing here drives processFrame or constructs a
// Game. The buffer is treated as a key/value store keyed by frame number,
// and we verify lookup, wrap-around, eviction, and the Predicted→Confirmed
// transition. The snapshot's `checksum` field is repurposed as an identity
// marker so we can tell which write produced which slot after a wrap.

#include <catch2/catch_test_macros.hpp>
#include <cstdint>

#include "rollback/buffer.hpp"

using rollback::RemoteState;
using rollback::RollbackBuffer;
using rollback::Slot;

TEST_CASE("RollbackBuffer is empty on construction", "[rollback]") {
  RollbackBuffer buf;
  REQUIRE(buf.Empty());
  REQUIRE(buf.Size() == 0);
  REQUIRE(buf.NewestFrame() == -1);
  REQUIRE(buf.OldestFrame() == -1);
  REQUIRE(buf.Find(0) == nullptr);
  REQUIRE(buf.Find(42) == nullptr);
}

TEST_CASE("RollbackBuffer stores and retrieves consecutive frames", "[rollback]") {
  RollbackBuffer buf;

  for (int f = 0; f < static_cast<int>(RollbackBuffer::kCapacity); ++f) {
    Slot& s = buf.Write(f);
    s.local_input = static_cast<uint8_t>(0x10 + f);
    s.remote_input = static_cast<uint8_t>(0x80 + f);
    s.remote_state = RemoteState::kConfirmed;
    s.snapshot.checksum = static_cast<uint32_t>(0xC0DE0000u + f);
  }

  REQUIRE(buf.Size() == RollbackBuffer::kCapacity);
  REQUIRE(buf.NewestFrame() == static_cast<int>(RollbackBuffer::kCapacity) - 1);
  REQUIRE(buf.OldestFrame() == 0);

  for (int f = 0; f < static_cast<int>(RollbackBuffer::kCapacity); ++f) {
    Slot const* s = buf.Find(f);
    REQUIRE(s != nullptr);
    REQUIRE(s->frame == f);
    REQUIRE(s->local_input == static_cast<uint8_t>(0x10 + f));
    REQUIRE(s->remote_input == static_cast<uint8_t>(0x80 + f));
    REQUIRE(s->remote_state == RemoteState::kConfirmed);
    REQUIRE(s->snapshot.checksum == static_cast<uint32_t>(0xC0DE0000u + f));
  }
}

TEST_CASE("RollbackBuffer wraps around and evicts oldest frame", "[rollback]") {
  RollbackBuffer buf;

  constexpr int kFrames = static_cast<int>(RollbackBuffer::kCapacity) * 3 + 2;
  for (int f = 0; f < kFrames; ++f) {
    Slot& s = buf.Write(f);
    s.local_input = static_cast<uint8_t>(f & 0xff);
    s.snapshot.checksum = static_cast<uint32_t>(0xA0000000u + f);
  }

  int const kNewest = kFrames - 1;
  int const kOldest = kNewest - static_cast<int>(RollbackBuffer::kCapacity) + 1;
  REQUIRE(buf.NewestFrame() == kNewest);
  REQUIRE(buf.OldestFrame() == kOldest);

  // Everything below the eviction horizon is gone.
  for (int f = 0; f < kOldest; ++f) {
    INFO("frame " << f << " should have been evicted");
    REQUIRE(buf.Find(f) == nullptr);
  }
  // Everything in [oldest, newest] is resident and carries the latest data.
  for (int f = kOldest; f <= kNewest; ++f) {
    Slot const* s = buf.Find(f);
    REQUIRE(s != nullptr);
    REQUIRE(s->frame == f);
    REQUIRE(s->local_input == static_cast<uint8_t>(f & 0xff));
    REQUIRE(s->snapshot.checksum == static_cast<uint32_t>(0xA0000000u + f));
  }
  // Future frames are not resident.
  REQUIRE(buf.Find(kNewest + 1) == nullptr);
}

TEST_CASE("RollbackBuffer write to existing frame preserves snapshot", "[rollback]") {
  // The controller will update remote input on a slot without re-saving the
  // snapshot (e.g. when arriving remote input matches the predicted input
  // for a still-resident frame). Repeated writes to the same frame must
  // leave the snapshot field untouched and not reset other fields when the
  // frame matches.
  RollbackBuffer buf;

  Slot& first = buf.Write(5);
  first.snapshot.checksum = 0xDEADBEEF;
  first.local_input = 0x11;
  first.remote_input = 0x22;
  first.remote_state = RemoteState::kPredicted;

  Slot& again = buf.Write(5);
  REQUIRE(&again == &first);
  REQUIRE(again.snapshot.checksum == 0xDEADBEEF);
  REQUIRE(again.local_input == 0x11);
  REQUIRE(again.remote_input == 0x22);
  REQUIRE(again.remote_state == RemoteState::kPredicted);

  // Mutate via the second handle, then look up via find: same slot.
  again.remote_input = 0x33;
  again.remote_state = RemoteState::kConfirmed;
  Slot const* found = buf.Find(5);
  REQUIRE(found == &first);
  REQUIRE(found->remote_input == 0x33);
  REQUIRE(found->remote_state == RemoteState::kConfirmed);
}

TEST_CASE("RollbackBuffer write to a different frame evicts ring slot", "[rollback]") {
  // Writing frame F+kCapacity must overwrite the slot that held F. Inputs
  // and remoteState reset to defaults so stale data from the evicted frame
  // cannot leak into the new one.
  RollbackBuffer buf;

  Slot& original = buf.Write(3);
  original.local_input = 0xAA;
  original.remote_input = 0xBB;
  original.remote_state = RemoteState::kConfirmed;
  original.snapshot.checksum = 0x12345678;

  int const kCollision = 3 + static_cast<int>(RollbackBuffer::kCapacity);
  Slot& replaced = buf.Write(kCollision);
  REQUIRE(&replaced == &original);  // same physical slot
  REQUIRE(replaced.frame == kCollision);
  REQUIRE(replaced.local_input == 0);
  REQUIRE(replaced.remote_input == 0);
  REQUIRE(replaced.remote_state == RemoteState::kPredicted);
  // Snapshot field is deliberately not cleared by write() — the controller
  // overwrites it via Game::saveSnapshotFast. Verify the carried-over
  // checksum is still there so callers can rely on this contract.
  REQUIRE(replaced.snapshot.checksum == 0x12345678);

  REQUIRE(buf.Find(3) == nullptr);
  REQUIRE(buf.Find(kCollision) == &original);
}

TEST_CASE("RollbackBuffer supports Predicted -> Confirmed transition", "[rollback]") {
  RollbackBuffer buf;
  for (int f = 0; f < 4; ++f) {
    Slot& s = buf.Write(f);
    s.remote_input = 0;
    s.remote_state = RemoteState::kPredicted;
  }

  // Remote input for frame 2 arrives.
  Slot* s = buf.Find(2);
  REQUIRE(s != nullptr);
  s->remote_input = 0x5A;
  s->remote_state = RemoteState::kConfirmed;

  // Lookup-by-frame after the transition reflects the change.
  Slot const* readback = buf.Find(2);
  REQUIRE(readback != nullptr);
  REQUIRE(readback->remote_state == RemoteState::kConfirmed);
  REQUIRE(readback->remote_input == 0x5A);

  // Other frames are unchanged.
  for (int f : {0, 1, 3}) {
    Slot const* other = buf.Find(f);
    REQUIRE(other != nullptr);
    REQUIRE(other->remote_state == RemoteState::kPredicted);
    REQUIRE(other->remote_input == 0);
  }
}

TEST_CASE("RollbackBuffer clear empties without losing capacity", "[rollback]") {
  RollbackBuffer buf;
  for (int f = 0; f < 5; ++f) buf.Write(f).local_input = 0x77;
  REQUIRE(!buf.Empty());

  buf.Clear();
  REQUIRE(buf.Empty());
  REQUIRE(buf.Size() == 0);
  REQUIRE(buf.NewestFrame() == -1);
  REQUIRE(buf.OldestFrame() == -1);
  for (int f = 0; f < 5; ++f) REQUIRE(buf.Find(f) == nullptr);

  // Still usable after clear.
  buf.Write(100).local_input = 0x99;
  REQUIRE(buf.NewestFrame() == 100);
  REQUIRE(buf.Find(100) != nullptr);
  REQUIRE(buf.Find(100)->local_input == 0x99);
}

TEST_CASE("RollbackBuffer reports oldestFrame correctly while filling", "[rollback]") {
  // Before the buffer is full, oldestFrame() is 0; after the first eviction
  // it tracks the eviction horizon.
  RollbackBuffer buf;
  for (int f = 0; f < static_cast<int>(RollbackBuffer::kCapacity); ++f) {
    buf.Write(f);
    REQUIRE(buf.OldestFrame() == 0);
    REQUIRE(buf.NewestFrame() == f);
  }
  // One more frame triggers eviction.
  buf.Write(static_cast<int>(RollbackBuffer::kCapacity));
  REQUIRE(buf.OldestFrame() == 1);
  REQUIRE(buf.NewestFrame() == static_cast<int>(RollbackBuffer::kCapacity));
}
