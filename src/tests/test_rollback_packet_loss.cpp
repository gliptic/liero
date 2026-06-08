// Packet loss survives via input redundancy.
//
// The wire format carries the last K = kMaxRollback + 1 inputs per
// packet, so a single dropped packet is covered by the next K-1
// packets. Under steady-state moderate loss the controllers should
// never stall: every frame eventually receives confirmed input within
// kMaxRollback frames of being run as a prediction.
//
// Setup: two RollbackControllers wired through JitterTransport with
// lossProbability = 0.10 and a small delay floor (so reorder is
// possible). Drive random inputs, then assert (a) both peers advance
// past a reasonable horizon, (b) the steady-state confirmation lag
// stays bounded by kMaxRollback, and (c) checksums agree at the end.

#include <algorithm>
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
  FsNode const kTcRoot(FsNode("data") / "TC" / "openliero");
  common->load(kTcRoot);
  auto settings = std::make_shared<Settings>();
  settings->lives = 10;
  settings->loading_time = 0;
  settings->load_change = true;
  settings->random_level = true;
  settings->game_mode = Settings::kGmKillEmAll;
  return {common, settings};
}

}  // namespace

TEST_CASE("Rollback survives 10% packet loss via input redundancy", "[rollback][loss]") {
  constexpr uint32_t kWorldSeed = 0xBEEF;
  constexpr int kTicks = 1500;
  constexpr uint32_t kInputSeed = 0xC0FFEE;

  auto [common, settings] = MakeEnv();
  auto a = std::make_unique<RollbackController>(common, settings, 0);
  auto b = std::make_unique<RollbackController>(common, settings, 1);
  a->SetSkipWeaponSelection(/*skip=*/true);
  b->SetSkipWeaponSelection(/*skip=*/true);
  // Frame-advantage stall is orthogonal to packet loss; turn it off so
  // the peers freely run ahead and we measure redundancy in isolation.
  a->SetFrameAdvantageEnabled(/*enabled=*/false);
  b->SetFrameAdvantageEnabled(/*enabled=*/false);
  a->game.rand.Seed(kWorldSeed);
  b->game.rand.Seed(kWorldSeed);

  rollback_test::JitterTransport transport({.seed = 0x10ADED,
                                            /*minDelay*/ .min_delay_frames = 1,
                                            /*maxDelay*/ .max_delay_frames = 3,
                                            /*lossProb*/ .loss_probability = 0.10,
                                            /*dupProb*/ .duplicate_probability = 0.0});

  a->SetInputCallbacks([&](uint8_t gen, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
    transport.SendAToB(gen, bf, c, in, lf);
  });
  b->SetInputCallbacks([&](uint8_t gen, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
    transport.SendBToA(gen, bf, c, in, lf);
  });
  a->Focus();
  b->Focus();

  for (uint32_t f = 0; f < 3; ++f) {
    a->InjectRemoteInput(f, 0);
    b->InjectRemoteInput(f, 0);
  }

  auto deliver_a = [&](uint8_t /*gen*/, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
    a->InjectRemoteBatch(bf, c, in, lf);
  };
  auto deliver_b = [&](uint8_t /*gen*/, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
    b->InjectRemoteBatch(bf, c, in, lf);
  };

  Rand input_rng(kInputSeed);
  uint32_t max_lag_a = 0;
  uint32_t max_lag_b = 0;
  int stall_ticks = 0;
  uint32_t prev_a = 0;
  uint32_t prev_b = 0;

  for (int tick = 0; tick < kTicks; ++tick) {
    uint8_t in_a = input_rng() & 0x7f;
    uint8_t in_b = input_rng() & 0x7f;
    if ((input_rng() % 10) < 6) {
      in_a |= (1 << Worm::kFire);
    }
    if ((input_rng() % 10) < 6) {
      in_b |= (1 << Worm::kFire);
    }
    a->SetLocalControlState(in_a);
    b->SetLocalControlState(in_b);
    a->Process();
    b->Process();
    transport.Tick(deliver_a, deliver_b);

    // Steady-state observation begins after the first ~50 ticks so the
    // warm-up doesn't pollute the running maxima.
    if (tick > 50) {
      uint32_t const kLagA = a->CurrentFrame() - static_cast<uint32_t>(a->ConfirmedFrame() + 1);
      uint32_t const kLagB = b->CurrentFrame() - static_cast<uint32_t>(b->ConfirmedFrame() + 1);
      max_lag_a = std::max(kLagA, max_lag_a);
      max_lag_b = std::max(kLagB, max_lag_b);
      if (a->CurrentFrame() == prev_a && b->CurrentFrame() == prev_b) {
        ++stall_ticks;
      }
    }
    prev_a = a->CurrentFrame();
    prev_b = b->CurrentFrame();
  }

  // Steady-state lag never reaches the stall threshold (kMaxRollback+1).
  // With K-wide redundancy a single dropped packet is covered by the
  // next packet, so the lag tops out at the natural network delay
  // plus a handful of redundant-covered drops.
  REQUIRE(max_lag_a <= static_cast<uint32_t>(rollback::kMaxRollback));
  REQUIRE(max_lag_b <= static_cast<uint32_t>(rollback::kMaxRollback));

  // The pair never enters a cascading stall — at least one peer
  // advances every tick under 10% loss.
  REQUIRE(stall_ticks == 0);

  // Loss should actually have fired; otherwise the test is vacuous.
  REQUIRE(transport.packets_dropped > 0);
  REQUIRE(a->RollbackCount() > 0);
  REQUIRE(b->RollbackCount() > 0);

  // Flush and drain, then assert checksums agree.
  transport.Flush(deliver_a, deliver_b);
  a->SetLocalControlState(0);
  b->SetLocalControlState(0);
  for (int i = 0; i < 16; ++i) {
    a->Process();
    b->Process();
    transport.Tick(deliver_a, deliver_b);
  }
  REQUIRE(a->CurrentFrame() == b->CurrentFrame());
  REQUIRE(WideRollbackChecksum(a->game) == WideRollbackChecksum(b->game));
}
