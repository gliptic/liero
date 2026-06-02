// Out-of-order delivery is handled idempotently.
//
// JitterTransport's per-packet random delay naturally lets a later-sent
// packet arrive ahead of an earlier-sent one. The receiver must apply
// each batch entry to the input ring via injectRemoteInput, where the
// stale-frame drop (frame <= confirmedSimFrame_) and idempotent write
// (same byte each time) keep state consistent regardless of arrival
// order. This test cranks up the delay range to maximise reordering and
// adds duplication on top.
//
// Verifies that despite the out-of-order arrivals:
//   - Both peers reach the same simFrame within the test horizon.
//   - Final state checksums match.
//   - Rollback actually fires (so we're not testing a vacuous fast path).

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <memory>

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

}  // namespace

TEST_CASE("Rollback survives reorder + duplication", "[rollback][reorder]") {
  constexpr uint32_t kWorldSeed = 0xBEEF;
  constexpr int kTicks = 1000;
  constexpr uint32_t kInputSeed = 0xC0FFEE;

  auto [common, settings] = MakeEnv();
  auto a = std::make_unique<RollbackController>(common, settings, 0);
  auto b = std::make_unique<RollbackController>(common, settings, 1);
  a->SetSkipWeaponSelection(true);
  b->SetSkipWeaponSelection(true);
  // Frame-advantage stall clamps the peers to lockstep and
  // suppresses the prediction window we need to exercise the reorder
  // path — disable it so we measure the dedup mechanism directly.
  a->SetFrameAdvantageEnabled(false);
  b->SetFrameAdvantageEnabled(false);
  a->game.rand.Seed(kWorldSeed);
  b->game.rand.Seed(kWorldSeed);

  // Wide delay band + duplication. With min=1, max=5 the variance is
  // large enough that earlier-sent packets routinely arrive after
  // later-sent ones; 30% duplication exercises the idempotent overwrite
  // path on remoteInputs slots and the stale-frame drop inside
  // injectRemoteInput. No loss here — that's covered by the loss test.
  rollback_test::JitterTransport transport({0xACE1, /*minDelay*/ 1, /*maxDelay*/ 5,
                                            /*lossProb*/ 0.0, /*dupProb*/ 0.30});

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

  auto deliver_a = [&](uint8_t gen, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
    a->InjectRemoteBatch(bf, c, in, lf);
  };
  auto deliver_b = [&](uint8_t gen, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
    b->InjectRemoteBatch(bf, c, in, lf);
  };

  Rand input_rng(kInputSeed);
  for (int tick = 0; tick < kTicks; ++tick) {
    uint8_t in_a = input_rng() & 0x7f;
    uint8_t in_b = input_rng() & 0x7f;
    if ((input_rng() % 10) < 6) in_a |= (1 << Worm::kFire);
    if ((input_rng() % 10) < 6) in_b |= (1 << Worm::kFire);
    a->SetLocalControlState(in_a);
    b->SetLocalControlState(in_b);
    a->Process();
    b->Process();
    transport.Tick(deliver_a, deliver_b);
  }

  transport.Flush(deliver_a, deliver_b);
  a->SetLocalControlState(0);
  b->SetLocalControlState(0);
  for (int i = 0; i < 16; ++i) {
    a->Process();
    b->Process();
    transport.Tick(deliver_a, deliver_b);
  }

  REQUIRE(transport.packets_duplicated > 0);
  REQUIRE(a->RollbackCount() > 0);
  REQUIRE(b->RollbackCount() > 0);
  REQUIRE(a->CurrentFrame() == b->CurrentFrame());
  REQUIRE(WideRollbackChecksum(a->game) == WideRollbackChecksum(b->game));
}
