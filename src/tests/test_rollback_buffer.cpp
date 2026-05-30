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
  REQUIRE(buf.empty());
  REQUIRE(buf.size() == 0);
  REQUIRE(buf.newestFrame() == -1);
  REQUIRE(buf.oldestFrame() == -1);
  REQUIRE(buf.find(0) == nullptr);
  REQUIRE(buf.find(42) == nullptr);
}

TEST_CASE("RollbackBuffer stores and retrieves consecutive frames",
          "[rollback]") {
  RollbackBuffer buf;

  for (int f = 0; f < static_cast<int>(RollbackBuffer::kCapacity); ++f) {
    Slot& s = buf.write(f);
    s.localInput = static_cast<uint8_t>(0x10 + f);
    s.remoteInput = static_cast<uint8_t>(0x80 + f);
    s.remoteState = RemoteState::Confirmed;
    s.snapshot.checksum = static_cast<uint32_t>(0xC0DE0000u + f);
  }

  REQUIRE(buf.size() == RollbackBuffer::kCapacity);
  REQUIRE(buf.newestFrame() ==
          static_cast<int>(RollbackBuffer::kCapacity) - 1);
  REQUIRE(buf.oldestFrame() == 0);

  for (int f = 0; f < static_cast<int>(RollbackBuffer::kCapacity); ++f) {
    Slot const* s = buf.find(f);
    REQUIRE(s != nullptr);
    REQUIRE(s->frame == f);
    REQUIRE(s->localInput == static_cast<uint8_t>(0x10 + f));
    REQUIRE(s->remoteInput == static_cast<uint8_t>(0x80 + f));
    REQUIRE(s->remoteState == RemoteState::Confirmed);
    REQUIRE(s->snapshot.checksum ==
            static_cast<uint32_t>(0xC0DE0000u + f));
  }
}

TEST_CASE("RollbackBuffer wraps around and evicts oldest frame",
          "[rollback]") {
  RollbackBuffer buf;

  constexpr int kFrames = static_cast<int>(RollbackBuffer::kCapacity) * 3 + 2;
  for (int f = 0; f < kFrames; ++f) {
    Slot& s = buf.write(f);
    s.localInput = static_cast<uint8_t>(f & 0xff);
    s.snapshot.checksum = static_cast<uint32_t>(0xA0000000u + f);
  }

  int const newest = kFrames - 1;
  int const oldest = newest - static_cast<int>(RollbackBuffer::kCapacity) + 1;
  REQUIRE(buf.newestFrame() == newest);
  REQUIRE(buf.oldestFrame() == oldest);

  // Everything below the eviction horizon is gone.
  for (int f = 0; f < oldest; ++f) {
    INFO("frame " << f << " should have been evicted");
    REQUIRE(buf.find(f) == nullptr);
  }
  // Everything in [oldest, newest] is resident and carries the latest data.
  for (int f = oldest; f <= newest; ++f) {
    Slot const* s = buf.find(f);
    REQUIRE(s != nullptr);
    REQUIRE(s->frame == f);
    REQUIRE(s->localInput == static_cast<uint8_t>(f & 0xff));
    REQUIRE(s->snapshot.checksum == static_cast<uint32_t>(0xA0000000u + f));
  }
  // Future frames are not resident.
  REQUIRE(buf.find(newest + 1) == nullptr);
}

TEST_CASE("RollbackBuffer write to existing frame preserves snapshot",
          "[rollback]") {
  // The controller will update remote input on a slot without re-saving the
  // snapshot (e.g. when arriving remote input matches the predicted input
  // for a still-resident frame). Repeated writes to the same frame must
  // leave the snapshot field untouched and not reset other fields when the
  // frame matches.
  RollbackBuffer buf;

  Slot& first = buf.write(5);
  first.snapshot.checksum = 0xDEADBEEF;
  first.localInput = 0x11;
  first.remoteInput = 0x22;
  first.remoteState = RemoteState::Predicted;

  Slot& again = buf.write(5);
  REQUIRE(&again == &first);
  REQUIRE(again.snapshot.checksum == 0xDEADBEEF);
  REQUIRE(again.localInput == 0x11);
  REQUIRE(again.remoteInput == 0x22);
  REQUIRE(again.remoteState == RemoteState::Predicted);

  // Mutate via the second handle, then look up via find: same slot.
  again.remoteInput = 0x33;
  again.remoteState = RemoteState::Confirmed;
  Slot const* found = buf.find(5);
  REQUIRE(found == &first);
  REQUIRE(found->remoteInput == 0x33);
  REQUIRE(found->remoteState == RemoteState::Confirmed);
}

TEST_CASE("RollbackBuffer write to a different frame evicts ring slot",
          "[rollback]") {
  // Writing frame F+kCapacity must overwrite the slot that held F. Inputs
  // and remoteState reset to defaults so stale data from the evicted frame
  // cannot leak into the new one.
  RollbackBuffer buf;

  Slot& original = buf.write(3);
  original.localInput = 0xAA;
  original.remoteInput = 0xBB;
  original.remoteState = RemoteState::Confirmed;
  original.snapshot.checksum = 0x12345678;

  int const collision = 3 + static_cast<int>(RollbackBuffer::kCapacity);
  Slot& replaced = buf.write(collision);
  REQUIRE(&replaced == &original);  // same physical slot
  REQUIRE(replaced.frame == collision);
  REQUIRE(replaced.localInput == 0);
  REQUIRE(replaced.remoteInput == 0);
  REQUIRE(replaced.remoteState == RemoteState::Predicted);
  // Snapshot field is deliberately not cleared by write() — the controller
  // overwrites it via Game::saveSnapshotFast. Verify the carried-over
  // checksum is still there so callers can rely on this contract.
  REQUIRE(replaced.snapshot.checksum == 0x12345678);

  REQUIRE(buf.find(3) == nullptr);
  REQUIRE(buf.find(collision) == &original);
}

TEST_CASE("RollbackBuffer supports Predicted -> Confirmed transition",
          "[rollback]") {
  RollbackBuffer buf;
  for (int f = 0; f < 4; ++f) {
    Slot& s = buf.write(f);
    s.remoteInput = 0;
    s.remoteState = RemoteState::Predicted;
  }

  // Remote input for frame 2 arrives.
  Slot* s = buf.find(2);
  REQUIRE(s != nullptr);
  s->remoteInput = 0x5A;
  s->remoteState = RemoteState::Confirmed;

  // Lookup-by-frame after the transition reflects the change.
  Slot const* readback = buf.find(2);
  REQUIRE(readback != nullptr);
  REQUIRE(readback->remoteState == RemoteState::Confirmed);
  REQUIRE(readback->remoteInput == 0x5A);

  // Other frames are unchanged.
  for (int f : {0, 1, 3}) {
    Slot const* other = buf.find(f);
    REQUIRE(other != nullptr);
    REQUIRE(other->remoteState == RemoteState::Predicted);
    REQUIRE(other->remoteInput == 0);
  }
}

TEST_CASE("RollbackBuffer clear empties without losing capacity",
          "[rollback]") {
  RollbackBuffer buf;
  for (int f = 0; f < 5; ++f) buf.write(f).localInput = 0x77;
  REQUIRE(!buf.empty());

  buf.clear();
  REQUIRE(buf.empty());
  REQUIRE(buf.size() == 0);
  REQUIRE(buf.newestFrame() == -1);
  REQUIRE(buf.oldestFrame() == -1);
  for (int f = 0; f < 5; ++f) REQUIRE(buf.find(f) == nullptr);

  // Still usable after clear.
  buf.write(100).localInput = 0x99;
  REQUIRE(buf.newestFrame() == 100);
  REQUIRE(buf.find(100) != nullptr);
  REQUIRE(buf.find(100)->localInput == 0x99);
}

TEST_CASE("RollbackBuffer reports oldestFrame correctly while filling",
          "[rollback]") {
  // Before the buffer is full, oldestFrame() is 0; after the first eviction
  // it tracks the eviction horizon.
  RollbackBuffer buf;
  for (int f = 0; f < static_cast<int>(RollbackBuffer::kCapacity); ++f) {
    buf.write(f);
    REQUIRE(buf.oldestFrame() == 0);
    REQUIRE(buf.newestFrame() == f);
  }
  // One more frame triggers eviction.
  buf.write(static_cast<int>(RollbackBuffer::kCapacity));
  REQUIRE(buf.oldestFrame() == 1);
  REQUIRE(buf.newestFrame() ==
          static_cast<int>(RollbackBuffer::kCapacity));
}
