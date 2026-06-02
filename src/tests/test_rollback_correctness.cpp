// Headline rollback-correctness test.
//
// Two RollbackControllers exchange inputs through a JitterTransport
// that delays delivery by a random amount within [minDelay, maxDelay]
// frames. Each peer predicts the remote side, advances speculatively,
// and reconciles via rollback when a disagreement arrives. After a
// convergence flush both peers must agree on every checksum still
// resident in the ring, and their state must match a zero-jitter
// reference run driven by the same inputs.
//
// The test uses small delays so the steady-state gap between simFrame
// and confirmedFrame stays comfortably under kMaxRollback even when
// runs of unlucky delays land back-to-back; larger delays + loss are
// covered by the redundancy / packet-loss tests.

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <memory>
#include <vector>

#include "controller/rollbackController.hpp"
#include "game.hpp"
#include "jitter_transport.hpp"
#include "math.hpp"
#include "mixer/player.hpp"
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
  return {common, settings};
}

struct ScriptedInputs {
  std::vector<uint8_t> a;
  std::vector<uint8_t> b;
};

ScriptedInputs GenerateInputs(uint32_t seed, int ticks) {
  Rand rng(seed);
  ScriptedInputs out;
  out.a.reserve(ticks);
  out.b.reserve(ticks);
  for (int i = 0; i < ticks; ++i) {
    uint8_t in_a = rng() & 0x7f;
    uint8_t in_b = rng() & 0x7f;
    if ((rng() % 10) < 6) in_a |= (1 << Worm::kFire);
    if ((rng() % 10) < 6) in_b |= (1 << Worm::kFire);
    out.a.push_back(in_a);
    out.b.push_back(in_b);
  }
  return out;
}

// Drive a zero-jitter reference run of `ticks` ticks with the given
// input sequence. Returns the final wideRollbackChecksum and the simFrame
// the peer reached.
struct RefResult {
  uint32_t checksum;
  uint32_t sim_frame;
};

RefResult RunReference(uint32_t world_seed, ScriptedInputs const& script, int ticks) {
  auto [common, settings] = MakeEnv();
  auto a = std::make_unique<RollbackController>(common, settings, 0);
  auto b = std::make_unique<RollbackController>(common, settings, 1);
  a->SetSkipWeaponSelection(true);
  b->SetSkipWeaponSelection(true);
  // Isolate the rollback algorithm from the frame-advantage stall so
  // the peers freely run ahead and exercise prediction.
  a->SetFrameAdvantageEnabled(false);
  b->SetFrameAdvantageEnabled(false);
  a->game.rand.Seed(world_seed);
  b->game.rand.Seed(world_seed);

  struct Pkt {
    uint32_t base_frame;
    uint8_t count;
    std::array<uint8_t, rollback::kMaxRollback + 1> inputs;
    uint32_t local_frame;
  };
  std::vector<Pkt> a_to_b, b_to_a;
  auto enqueue = [](std::vector<Pkt>& q, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
    Pkt p{};
    p.base_frame = bf;
    p.count = c;
    p.local_frame = lf;
    for (uint8_t i = 0; i < c; ++i) p.inputs[i] = in[i];
    q.push_back(p);
  };
  a->SetInputCallbacks([&](uint8_t gen, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
    enqueue(a_to_b, bf, c, in, lf);
  });
  b->SetInputCallbacks([&](uint8_t gen, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
    enqueue(b_to_a, bf, c, in, lf);
  });
  a->Focus();
  b->Focus();

  for (uint32_t f = 0; f < 3; ++f) {
    a->InjectRemoteInput(f, 0);
    b->InjectRemoteInput(f, 0);
  }

  for (int i = 0; i < ticks; ++i) {
    a->SetLocalControlState(script.a[i]);
    b->SetLocalControlState(script.b[i]);
    a->Process();
    b->Process();
    for (auto const& p : a_to_b)
      b->InjectRemoteBatch(p.base_frame, p.count, p.inputs.data(), p.local_frame);
    for (auto const& p : b_to_a)
      a->InjectRemoteBatch(p.base_frame, p.count, p.inputs.data(), p.local_frame);
    a_to_b.clear();
    b_to_a.clear();
  }

  REQUIRE(a->CurrentFrame() == b->CurrentFrame());
  uint32_t c_a = WideRollbackChecksum(a->game);
  uint32_t c_b = WideRollbackChecksum(b->game);
  REQUIRE(c_a == c_b);
  return {c_a, a->CurrentFrame()};
}

}  // namespace

TEST_CASE("Rollback recovers from mispredictions under random delay", "[rollback][correctness]") {
  constexpr uint32_t kWorldSeed = 0xBEEF;
  constexpr int kTicks = 800;
  constexpr uint32_t kInputSeed = 0xC0FFEE;

  ScriptedInputs script = GenerateInputs(kInputSeed, kTicks);
  RefResult ref = RunReference(kWorldSeed, script, kTicks);

  struct Case {
    char const* name;
    int min_delay;
    int max_delay;
    uint32_t transport_seed;
  };
  // Delays are chosen so the steady-state gap stays well under
  // kMaxRollback even when unlucky runs of high-delay packets cluster.
  std::vector<Case> cases = {
      {"delay [1,3]", 1, 3, 0x1111},
      {"delay [1,4]", 1, 4, 0x2222},
      {"delay [2,4]", 2, 4, 0x3333},
      {"delay [1,5]", 1, 5, 0x4444},
  };

  for (auto const& tc : cases) {
    SECTION(tc.name) {
      INFO("transport seed = " << tc.transport_seed);

      auto [common, settings] = MakeEnv();
      auto a = std::make_unique<RollbackController>(common, settings, 0);
      auto b = std::make_unique<RollbackController>(common, settings, 1);
      a->SetSkipWeaponSelection(true);
      b->SetSkipWeaponSelection(true);
      // Disable the frame-advantage stall so the rollback algorithm
      // is exercised under jitter without the time-sync clamp.
      a->SetFrameAdvantageEnabled(false);
      b->SetFrameAdvantageEnabled(false);
      a->game.rand.Seed(kWorldSeed);
      b->game.rand.Seed(kWorldSeed);

      rollback_test::JitterTransport transport({tc.transport_seed, tc.min_delay, tc.max_delay});

      a->SetInputCallbacks([&](uint8_t gen, uint32_t bf, uint8_t c, uint8_t const* in,
                               uint32_t lf) { transport.SendAToB(gen, bf, c, in, lf); });
      b->SetInputCallbacks([&](uint8_t gen, uint32_t bf, uint8_t c, uint8_t const* in,
                               uint32_t lf) { transport.SendBToA(gen, bf, c, in, lf); });
      a->Focus();
      b->Focus();

      // Pre-seed the input-delay window. These bypass the transport so
      // both peers can start advancing immediately, mirroring how a real
      // session would synchronise its starting frames.
      for (uint32_t f = 0; f < 3; ++f) {
        a->InjectRemoteInput(f, 0);
        b->InjectRemoteInput(f, 0);
      }

      auto deliver_a = [&](uint8_t gen, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
        a->InjectRemoteBatch(gen, bf, c, in, lf);
      };
      auto deliver_b = [&](uint8_t gen, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
        b->InjectRemoteBatch(gen, bf, c, in, lf);
      };

      // Track that lastTickResimFrames() goes non-zero at some point
      // during the run, catching a regression where rollback counting
      // falls back to the cumulative rollbackCount() metric.
      uint32_t max_resim_in_a_tick = 0;
      for (int i = 0; i < kTicks; ++i) {
        a->SetLocalControlState(script.a[i]);
        b->SetLocalControlState(script.b[i]);
        a->Process();
        b->Process();
        if (a->LastTickResimFrames() > max_resim_in_a_tick)
          max_resim_in_a_tick = a->LastTickResimFrames();
        if (b->LastTickResimFrames() > max_resim_in_a_tick)
          max_resim_in_a_tick = b->LastTickResimFrames();
        transport.Tick(deliver_a, deliver_b);
      }
      REQUIRE(max_resim_in_a_tick > 0);

      // Flush any in-flight tail packets, then run additional ticks
      // until both peers' confirmedFrame catches up. We use idle inputs
      // for the catch-up window so the test doesn't outrun the script,
      // but the comparison frame is the final scripted-tick state.
      transport.Flush(deliver_a, deliver_b);

      // Both peers must agree on every still-resident slot's
      // post-resim state. After flush, the promote loop on the next
      // tick of each peer will absorb the late arrivals.
      a->SetLocalControlState(0);
      b->SetLocalControlState(0);
      // A couple of extra ticks lets the promote loop drain.
      for (int i = 0; i < 12; ++i) {
        a->Process();
        b->Process();
        transport.Tick(deliver_a, deliver_b);
      }

      REQUIRE(a->CurrentFrame() == b->CurrentFrame());

      // Sanity: the test would trivially pass if rollback never fired
      // (predictions always matching means the algorithm wasn't exercised).
      // With random inputs and any delay, mispredictions are inevitable.
      REQUIRE(a->RollbackCount() > 0);
      REQUIRE(b->RollbackCount() > 0);

      uint32_t c_a = WideRollbackChecksum(a->game);
      uint32_t c_b = WideRollbackChecksum(b->game);
      REQUIRE(c_a == c_b);

      // And it has to match what a zero-jitter peer would have produced
      // from the same input sequence at the same frame. The reference
      // ran `kTicks` ticks; this peer ran `kTicks` scripted + 12 idle.
      // Run the reference the same number of idle ticks so the
      // comparison frames line up.
      auto ref_advanced = RunReference(
          kWorldSeed,
          [&] {
            ScriptedInputs ext = script;
            for (int i = 0; i < 12; ++i) {
              ext.a.push_back(0);
              ext.b.push_back(0);
            }
            return ext;
          }(),
          kTicks + 12);
      REQUIRE(a->CurrentFrame() == ref_advanced.sim_frame);
      REQUIRE(c_a == ref_advanced.checksum);
    }
  }
}
