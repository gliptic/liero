// Force-skew regression gate.
//
// Drives two RollbackControllers through weapon select over asymmetric
// one-way delays, deliberately producing a WS-phase simFrame skew
// between the peers. Asserts:
//   * during WS, the peers were observed at different simFrame values
//     at least once (so the test isn't vacuous);
//   * after the WS→game transition, both peers land at simFrame=0;
//   * the cached wideRollbackChecksum on rollback slots for the first
//     ~32 game-phase frames matches between the peers.
//
// Removing the post-WS reset (resetForGamePhase + finishWeaponSelect)
// makes the post-transition simFrame assertion fail: each peer would
// transition at whatever simFrame WS reached on its side, and the
// asymmetry would persist into the game phase.

#include <algorithm>
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <memory>
#include <vector>

#include "controller/rollbackController.hpp"
#include "game.hpp"
#include "jitter_transport.hpp"
#include "math.hpp"
#include "rollback/buffer.hpp"

namespace {

std::pair<std::shared_ptr<Common>, std::shared_ptr<Settings>> MakeEnv() {
  PrecomputeTables();
  auto common = std::make_shared<Common>();
  FsNode tc_root(FsNode("data") / "TC" / "openliero");
  common->load(std::move(tc_root));
  auto settings = std::make_shared<Settings>();
  settings->lives = 10;
  settings->loading_time = 0;
  settings->load_change = true;
  settings->random_level = true;
  settings->game_mode = Settings::kGmKillEmAll;
  settings->select_bot_weapons = 0;
  return {common, settings};
}

std::unique_ptr<RollbackController> MakePeer(std::shared_ptr<Common> common,
                                             std::shared_ptr<Settings> settings, int local_idx,
                                             uint32_t world_seed) {
  auto c = std::make_unique<RollbackController>(common, settings, local_idx);
  c->SetInputDelay(1);
  c->game.rand.Seed(world_seed);
  return c;
}

constexpr uint8_t kBitDown = uint8_t{1} << Worm::kDown;
constexpr uint8_t kBitFire = uint8_t{1} << Worm::kFire;

std::vector<uint8_t> NavigateToDoneAndConfirm(int n_down) {
  std::vector<uint8_t> out;
  for (int i = 0; i < n_down; ++i) {
    out.push_back(kBitDown);
    out.push_back(0);
  }
  out.push_back(0);
  out.push_back(kBitFire);
  out.push_back(0);
  return out;
}

}  // namespace

TEST_CASE("WS simFrame skew is erased at the WS→game transition", "[rollback][step14][skew]") {
  constexpr uint32_t kWorldSeed = 0xF00DBABE;
  auto [common, settings] = MakeEnv();
  auto a = MakePeer(common, settings, 0, kWorldSeed);
  auto b = MakePeer(common, settings, 1, kWorldSeed);

  // Asymmetric delays — A→B fast (1 frame), B→A slow (4 frames). Plus
  // we disable A's frame-advantage stall so A runs ahead freely while B
  // (which still stalls when its view of A's simFrame lags) progresses
  // slower. The combination reliably produces a multi-frame WS-phase
  // skew while keeping both peers progressing enough to eventually
  // confirm wsDone.
  rollback_test::JitterTransport t_ab(
      {0x1111, /*minDelay=*/1, /*maxDelay=*/1, /*loss=*/0.0, /*dup=*/0.0});
  rollback_test::JitterTransport t_ba(
      {0x2222, /*minDelay=*/4, /*maxDelay=*/4, /*loss=*/0.0, /*dup=*/0.0});
  a->SetFrameAdvantageEnabled(false);

  a->SetInputCallbacks([&](uint8_t gen, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
    t_ab.SendAToB(gen, bf, c, in, lf);
  });
  b->SetInputCallbacks([&](uint8_t gen, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
    t_ba.SendAToB(gen, bf, c, in, lf);
  });

  a->Focus();
  b->Focus();
  a->InjectRemoteInput(0, 0);
  b->InjectRemoteInput(0, 0);

  auto deliver_b = [&](uint8_t gen, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
    b->InjectRemoteBatch(gen, bf, c, in, lf);
  };
  auto deliver_a = [&](uint8_t gen, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
    a->InjectRemoteBatch(gen, bf, c, in, lf);
  };
  auto noop = [&](uint8_t, uint32_t, uint8_t, uint8_t const*, uint32_t) {};

  auto script = NavigateToDoneAndConfirm(6);
  constexpr int kWsTail = 80;  // extra ticks so the slower peer catches up

  bool a_transitioned = false, b_transitioned = false;
  uint32_t a_transition_frame = 0, b_transition_frame = 0;
  int max_observed_skew = 0;

  auto step_both = [&](uint8_t in_a, uint8_t in_b) {
    a->SetLocalControlState(in_a);
    b->SetLocalControlState(in_b);
    bool a_pre = !a_transitioned;
    bool b_pre = !b_transitioned;
    a->Process();
    b->Process();
    if (a_pre && a->State() == kStateGame) {
      a_transitioned = true;
      a_transition_frame = a->CurrentFrame();
    }
    if (b_pre && b->State() == kStateGame) {
      b_transitioned = true;
      b_transition_frame = b->CurrentFrame();
    }
    // tAB carries A→B; the "deliverA" leg of its tick is unused.
    t_ab.Tick(noop, deliver_b);
    t_ba.Tick(noop, deliver_a);
  };

  for (int i = 0;
       i < static_cast<int>(script.size()) + kWsTail && !(a_transitioned && b_transitioned); ++i) {
    uint8_t in = (i < static_cast<int>(script.size())) ? script[i] : 0;
    step_both(in, in);
    // Observe WS-phase skew while at least one peer is still pre-transition.
    if (!a_transitioned || !b_transitioned) {
      int gap = static_cast<int>(a->CurrentFrame()) - static_cast<int>(b->CurrentFrame());
      if (gap < 0) gap = -gap;
      if (gap > max_observed_skew) max_observed_skew = gap;
    }
  }

  // Flush any tail packets, then a few idle ticks to finish either peer
  // that hasn't transitioned yet.
  t_ab.Flush(noop, deliver_b);
  t_ba.Flush(noop, deliver_a);
  for (int i = 0; i < 24 && !(a_transitioned && b_transitioned); ++i) {
    step_both(0, 0);
  }

  INFO("maxObservedSkew=" << max_observed_skew << " aTransitionFrame=" << a_transition_frame
                          << " bTransitionFrame=" << b_transition_frame);
  REQUIRE(a_transitioned);
  REQUIRE(b_transitioned);

  // Headline: regardless of WS skew, both peers entered game phase at
  // simFrame=0. Removing the post-WS reset would leave the peers at
  // whatever simFrame WS reached on each side.
  REQUIRE(a_transition_frame == 0);
  REQUIRE(b_transition_frame == 0);

  // Vacuity guard: if we never observed a WS-phase skew the test isn't
  // exercising the bug class. The asymmetric delay + disabled
  // frame-advantage on A routinely produces a multi-frame gap; a
  // single-frame gap is still meaningful evidence the timeline went
  // asymmetric.
  REQUIRE(max_observed_skew >= 1);

  // Drive ~32 game-phase frames in sync, then a flush + short drain.
  // Both peers stay idle so the sim produces identical state on each
  // side; the rollback path exercises predicted/confirmed transitions
  // because the B→A pipe still delays inputs by 4 frames.
  constexpr int kGameFrames = 32;
  for (int i = 0; i < kGameFrames; ++i) {
    step_both(0, 0);
  }
  t_ab.Flush(noop, deliver_b);
  t_ba.Flush(noop, deliver_a);
  for (int i = 0; i < 16; ++i) {
    step_both(0, 0);
  }

  // Cached wideRollbackChecksum match across the slots currently
  // resident in both peers' rings. The ring is bounded at
  // kMaxRollback+1 = 8 slots, so we can't compare "frame 0 to 31"
  // verbatim — we compare the overlap of the two rings' current
  // contents, which after the drive above is the most recent ~8
  // confirmed frames on each side. Together with the in-loop
  // checksumming this guards the first ~32 game-phase frames'
  // determinism (any earlier mismatch would propagate forward into
  // the still-resident frames).
  rollback::RollbackBuffer const& buf_a = a->RollbackBuffer();
  rollback::RollbackBuffer const& buf_b = b->RollbackBuffer();
  int compared = 0;
  int lo_f = std::max(buf_a.OldestFrame(), buf_b.OldestFrame());
  int hi_f = std::min(buf_a.NewestFrame(), buf_b.NewestFrame());
  for (int f = lo_f; f <= hi_f; ++f) {
    auto* slot_a = const_cast<rollback::RollbackBuffer&>(buf_a).Find(f);
    auto* slot_b = const_cast<rollback::RollbackBuffer&>(buf_b).Find(f);
    if (!slot_a || !slot_b) continue;
    INFO("frame " << f << " A=" << slot_a->checksum << " B=" << slot_b->checksum);
    REQUIRE(slot_a->checksum == slot_b->checksum);
    ++compared;
  }
  // Vacuity guard.
  REQUIRE(compared >= 4);
}
