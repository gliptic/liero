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

std::pair<std::shared_ptr<Common>, std::shared_ptr<Settings>> makeEnv() {
  precomputeTables();
  auto common = std::make_shared<Common>();
  FsNode tcRoot(FsNode("data") / "TC" / "openliero");
  common->load(std::move(tcRoot));
  auto settings = std::make_shared<Settings>();
  settings->lives = 10;
  settings->loadingTime = 0;
  settings->loadChange = true;
  settings->randomLevel = true;
  settings->gameMode = Settings::GMKillEmAll;
  return {common, settings};
}

struct ScriptedInputs {
  std::vector<uint8_t> a;
  std::vector<uint8_t> b;
};

ScriptedInputs generateInputs(uint32_t seed, int ticks) {
  Rand rng(seed);
  ScriptedInputs out;
  out.a.reserve(ticks);
  out.b.reserve(ticks);
  for (int i = 0; i < ticks; ++i) {
    uint8_t inA = rng() & 0x7f;
    uint8_t inB = rng() & 0x7f;
    if ((rng() % 10) < 6) inA |= (1 << Worm::Fire);
    if ((rng() % 10) < 6) inB |= (1 << Worm::Fire);
    out.a.push_back(inA);
    out.b.push_back(inB);
  }
  return out;
}

// Drive a zero-jitter reference run of `ticks` ticks with the given
// input sequence. Returns the final wideRollbackChecksum and the simFrame
// the peer reached.
struct RefResult {
  uint32_t checksum;
  uint32_t simFrame;
};

RefResult runReference(uint32_t worldSeed, ScriptedInputs const& script, int ticks) {
  auto [common, settings] = makeEnv();
  auto a = std::make_unique<RollbackController>(common, settings, 0);
  auto b = std::make_unique<RollbackController>(common, settings, 1);
  a->setSkipWeaponSelection(true);
  b->setSkipWeaponSelection(true);
  // Isolate the rollback algorithm from the frame-advantage stall so
  // the peers freely run ahead and exercise prediction.
  a->setFrameAdvantageEnabled(false);
  b->setFrameAdvantageEnabled(false);
  a->game.rand.seed(worldSeed);
  b->game.rand.seed(worldSeed);

  struct Pkt {
    uint32_t baseFrame;
    uint8_t count;
    std::array<uint8_t, rollback::kMaxRollback + 1> inputs;
    uint32_t localFrame;
  };
  std::vector<Pkt> aToB, bToA;
  auto enqueue = [](std::vector<Pkt>& q, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
    Pkt p{};
    p.baseFrame = bf;
    p.count = c;
    p.localFrame = lf;
    for (uint8_t i = 0; i < c; ++i) p.inputs[i] = in[i];
    q.push_back(p);
  };
  a->setInputCallbacks([&](uint8_t gen_, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
    enqueue(aToB, bf, c, in, lf);
  });
  b->setInputCallbacks([&](uint8_t gen_, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
    enqueue(bToA, bf, c, in, lf);
  });
  a->focus();
  b->focus();

  for (uint32_t f = 0; f < 3; ++f) {
    a->injectRemoteInput(f, 0);
    b->injectRemoteInput(f, 0);
  }

  for (int i = 0; i < ticks; ++i) {
    a->setLocalControlState(script.a[i]);
    b->setLocalControlState(script.b[i]);
    a->process();
    b->process();
    for (auto const& p : aToB)
      b->injectRemoteBatch(p.baseFrame, p.count, p.inputs.data(), p.localFrame);
    for (auto const& p : bToA)
      a->injectRemoteBatch(p.baseFrame, p.count, p.inputs.data(), p.localFrame);
    aToB.clear();
    bToA.clear();
  }

  REQUIRE(a->currentFrame() == b->currentFrame());
  uint32_t cA = wideRollbackChecksum(a->game);
  uint32_t cB = wideRollbackChecksum(b->game);
  REQUIRE(cA == cB);
  return {cA, a->currentFrame()};
}

}  // namespace

TEST_CASE("Rollback recovers from mispredictions under random delay", "[rollback][correctness]") {
  constexpr uint32_t kWorldSeed = 0xBEEF;
  constexpr int kTicks = 800;
  constexpr uint32_t kInputSeed = 0xC0FFEE;

  ScriptedInputs script = generateInputs(kInputSeed, kTicks);
  RefResult ref = runReference(kWorldSeed, script, kTicks);

  struct Case {
    char const* name;
    int minDelay;
    int maxDelay;
    uint32_t transportSeed;
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
      INFO("transport seed = " << tc.transportSeed);

      auto [common, settings] = makeEnv();
      auto a = std::make_unique<RollbackController>(common, settings, 0);
      auto b = std::make_unique<RollbackController>(common, settings, 1);
      a->setSkipWeaponSelection(true);
      b->setSkipWeaponSelection(true);
      // Disable the frame-advantage stall so the rollback algorithm
      // is exercised under jitter without the time-sync clamp.
      a->setFrameAdvantageEnabled(false);
      b->setFrameAdvantageEnabled(false);
      a->game.rand.seed(kWorldSeed);
      b->game.rand.seed(kWorldSeed);

      rollback_test::JitterTransport transport({tc.transportSeed, tc.minDelay, tc.maxDelay});

      a->setInputCallbacks([&](uint8_t gen_, uint32_t bf, uint8_t c, uint8_t const* in,
                               uint32_t lf) { transport.sendAToB(gen_, bf, c, in, lf); });
      b->setInputCallbacks([&](uint8_t gen_, uint32_t bf, uint8_t c, uint8_t const* in,
                               uint32_t lf) { transport.sendBToA(gen_, bf, c, in, lf); });
      a->focus();
      b->focus();

      // Pre-seed the input-delay window. These bypass the transport so
      // both peers can start advancing immediately, mirroring how a real
      // session would synchronise its starting frames.
      for (uint32_t f = 0; f < 3; ++f) {
        a->injectRemoteInput(f, 0);
        b->injectRemoteInput(f, 0);
      }

      auto deliverA = [&](uint8_t gen, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
        a->injectRemoteBatch(gen, bf, c, in, lf);
      };
      auto deliverB = [&](uint8_t gen, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
        b->injectRemoteBatch(gen, bf, c, in, lf);
      };

      // Track that lastTickResimFrames() goes non-zero at some point
      // during the run, catching a regression where rollback counting
      // falls back to the cumulative rollbackCount() metric.
      uint32_t maxResimInATick = 0;
      for (int i = 0; i < kTicks; ++i) {
        a->setLocalControlState(script.a[i]);
        b->setLocalControlState(script.b[i]);
        a->process();
        b->process();
        if (a->lastTickResimFrames() > maxResimInATick) maxResimInATick = a->lastTickResimFrames();
        if (b->lastTickResimFrames() > maxResimInATick) maxResimInATick = b->lastTickResimFrames();
        transport.tick(deliverA, deliverB);
      }
      REQUIRE(maxResimInATick > 0);

      // Flush any in-flight tail packets, then run additional ticks
      // until both peers' confirmedFrame catches up. We use idle inputs
      // for the catch-up window so the test doesn't outrun the script,
      // but the comparison frame is the final scripted-tick state.
      transport.flush(deliverA, deliverB);

      // Both peers must agree on every still-resident slot's
      // post-resim state. After flush, the promote loop on the next
      // tick of each peer will absorb the late arrivals.
      a->setLocalControlState(0);
      b->setLocalControlState(0);
      // A couple of extra ticks lets the promote loop drain.
      for (int i = 0; i < 12; ++i) {
        a->process();
        b->process();
        transport.tick(deliverA, deliverB);
      }

      REQUIRE(a->currentFrame() == b->currentFrame());

      // Sanity: the test would trivially pass if rollback never fired
      // (predictions always matching means the algorithm wasn't exercised).
      // With random inputs and any delay, mispredictions are inevitable.
      REQUIRE(a->rollbackCount() > 0);
      REQUIRE(b->rollbackCount() > 0);

      uint32_t cA = wideRollbackChecksum(a->game);
      uint32_t cB = wideRollbackChecksum(b->game);
      REQUIRE(cA == cB);

      // And it has to match what a zero-jitter peer would have produced
      // from the same input sequence at the same frame. The reference
      // ran `kTicks` ticks; this peer ran `kTicks` scripted + 12 idle.
      // Run the reference the same number of idle ticks so the
      // comparison frames line up.
      auto refAdvanced = runReference(
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
      REQUIRE(a->currentFrame() == refAdvanced.simFrame);
      REQUIRE(cA == refAdvanced.checksum);
    }
  }
}
