// Prediction without rollback.
//
// Two properties verified:
//   1. Zero jitter: prediction never kicks in; every snapshot slot is
//      Confirmed and confirmedFrame keeps up with simFrame-1.
//   2. Starvation: when remote input stops arriving, the controller
//      predicts up to kMaxRollback frames using the last received input
//      and then stalls; when missing inputs arrive the stall lifts and
//      confirmedFrame catches up.

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <memory>

#include "controller/rollbackController.hpp"
#include "game.hpp"
#include "math.hpp"
#include "mixer/player.hpp"
#include "rollback/buffer.hpp"

namespace {

// Zero-jitter loopback: each peer's batched send fans straight into the
// other peer's injectRemoteInput. The dedup inside injectRemoteInput
// absorbs the redundant entries naturally; this is what a real session
// would do once a batch packet is parsed.
template <typename Ctrl>
struct Loopback {
  std::shared_ptr<Common> common;
  std::shared_ptr<Settings> settings;
  std::unique_ptr<Ctrl> a;
  std::unique_ptr<Ctrl> b;

  Loopback(std::shared_ptr<Common> common_in, std::shared_ptr<Settings> settings_in, uint32_t seed)
      : common(std::move(common_in)), settings(std::move(settings_in)) {
    a = std::make_unique<Ctrl>(common, settings, 0);
    b = std::make_unique<Ctrl>(common, settings, 1);
    a->SetSkipWeaponSelection(true);
    b->SetSkipWeaponSelection(true);
    a->game.rand.Seed(seed);
    b->game.rand.Seed(seed);
    a->SetInputCallbacks([this](uint8_t /*gen*/, uint32_t bf, uint8_t c, uint8_t const* inputs,
                                uint32_t lf) { b->InjectRemoteBatch(bf, c, inputs, lf); });
    b->SetInputCallbacks([this](uint8_t /*gen*/, uint32_t bf, uint8_t c, uint8_t const* inputs,
                                uint32_t lf) { a->InjectRemoteBatch(bf, c, inputs, lf); });
    a->Focus();
    b->Focus();
  }

  // No-op now that send delivers directly; kept so the test reads as a
  // tick loop even though there's nothing queued.
  void DeliverAll() {}
};

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

TEST_CASE("Zero jitter: predictions never trigger", "[rollback][prediction]") {
  auto [common, settings] = MakeEnv();
  Loopback<RollbackController> rb(common, settings, 0xBEEF);

  // Pre-seed the 3-frame input-delay window.
  for (uint32_t i = 0; i < 3; ++i) {
    rb.a->InjectRemoteInput(i, 0);
    rb.b->InjectRemoteInput(i, 0);
  }

  Rand input_rng(0xC0FFEE);
  constexpr int kTicks = 500;
  for (int tick = 0; tick < kTicks; ++tick) {
    uint8_t in_a = input_rng() & 0x7f;
    uint8_t in_b = input_rng() & 0x7f;
    if ((input_rng() % 10) < 6) in_a |= (1 << Worm::kFire);
    if ((input_rng() % 10) < 6) in_b |= (1 << Worm::kFire);
    rb.a->SetLocalControlState(in_a);
    rb.b->SetLocalControlState(in_b);

    rb.a->Process();
    rb.b->Process();
    rb.DeliverAll();
  }

  // confirmedFrame should track simFrame-1 exactly under zero jitter.
  REQUIRE(rb.a->ConfirmedFrame() == static_cast<int32_t>(rb.a->CurrentFrame()) - 1);
  REQUIRE(rb.b->ConfirmedFrame() == static_cast<int32_t>(rb.b->CurrentFrame()) - 1);

  // Every resident slot is Confirmed.
  auto const& buf_a = rb.a->RollbackBuffer();
  for (int f = buf_a.OldestFrame(); f <= buf_a.NewestFrame(); ++f) {
    auto* slot = const_cast<rollback::RollbackBuffer&>(buf_a).Find(f);
    REQUIRE(slot != nullptr);
    REQUIRE(slot->remote_state == rollback::RemoteState::kConfirmed);
  }
}

TEST_CASE("Starvation: predict up to kMaxRollback, then stall, then recover",
          "[rollback][prediction]") {
  auto [common, settings] = MakeEnv();
  // Drive A directly via injectRemoteInput; B is unused. No transport.
  RollbackController a(common, settings, 0);
  a.SetSkipWeaponSelection(true);
  a.game.rand.Seed(0x1234);
  a.Focus();

  // Seed remote inputs for exactly the warm-up frames so A advances in
  // confirmed lockstep up to kWarm and no further.
  constexpr uint32_t kWarm = 20;
  for (uint32_t f = 0; f < kWarm; ++f) {
    a.InjectRemoteInput(f, 0);
  }
  for (uint32_t i = 0; i < kWarm; ++i) {
    a.SetLocalControlState(0);
    a.Process();
  }
  REQUIRE(a.CurrentFrame() == kWarm);
  REQUIRE(a.ConfirmedFrame() == static_cast<int32_t>(kWarm) - 1);

  // Stop delivering remote inputs. A predicts up to kMaxRollback frames
  // ahead and then stalls (subsequent process() calls become no-ops on
  // simFrame).
  constexpr int kStarveTicks = rollback::kMaxRollback + 5;
  uint32_t frame_at_starve_start = a.CurrentFrame();
  for (int i = 0; i < kStarveTicks; ++i) {
    a.SetLocalControlState(0);
    a.Process();
  }
  // Bound: simFrame advanced by at most kMaxRollback past confirmedFrame.
  REQUIRE(a.CurrentFrame() == frame_at_starve_start + rollback::kMaxRollback);
  REQUIRE(a.ConfirmedFrame() == static_cast<int32_t>(frame_at_starve_start) - 1);

  // The predicted frames are marked Predicted in the buffer.
  auto const& buf = a.RollbackBuffer();
  int predicted_count = 0, confirmed_count = 0;
  for (int f = buf.OldestFrame(); f <= buf.NewestFrame(); ++f) {
    auto* slot = const_cast<rollback::RollbackBuffer&>(buf).Find(f);
    REQUIRE(slot != nullptr);
    if (slot->remote_state == rollback::RemoteState::kPredicted)
      ++predicted_count;
    else
      ++confirmed_count;
  }
  REQUIRE(predicted_count == rollback::kMaxRollback);
  // Buffer holds kMaxRollback+1 slots total → 1 confirmed straggler.
  REQUIRE(confirmed_count == 1);

  // Late delivery: inject the previously-missing remote inputs. The
  // promote step on the next process() should catch confirmedFrame up
  // to simFrame-1 (we sent idle 0, predicted idle 0 — they match in
  // *input* terms).
  uint32_t sim_frame_at_stall = a.CurrentFrame();
  for (uint32_t f = frame_at_starve_start; f < sim_frame_at_stall; ++f) {
    a.InjectRemoteInput(f, 0);
  }
  // One more process() drains the late-arriving inputs via the promote
  // step. Then a second tick can actually advance again because we keep
  // delivering input-delay frames (we also inject inputs for the next
  // few frames so the new frame can be Confirmed-advanced).
  for (uint32_t f = sim_frame_at_stall; f < sim_frame_at_stall + 3; ++f) {
    a.InjectRemoteInput(f, 0);
  }
  a.SetLocalControlState(0);
  a.Process();

  REQUIRE(a.ConfirmedFrame() >= static_cast<int32_t>(sim_frame_at_stall) - 1);
  REQUIRE(a.CurrentFrame() > sim_frame_at_stall);
}
