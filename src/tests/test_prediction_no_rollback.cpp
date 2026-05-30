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

  Loopback(std::shared_ptr<Common> commonIn,
           std::shared_ptr<Settings> settingsIn,
           uint32_t seed)
      : common(std::move(commonIn)), settings(std::move(settingsIn)) {
    a = std::make_unique<Ctrl>(common, settings, 0);
    b = std::make_unique<Ctrl>(common, settings, 1);
    a->setSkipWeaponSelection(true);
    b->setSkipWeaponSelection(true);
    a->game.rand.seed(seed);
    b->game.rand.seed(seed);
    a->setInputCallbacks(
        [this](uint8_t /*gen*/, uint32_t bf, uint8_t c,
               uint8_t const* inputs, uint32_t lf) {
          b->injectRemoteBatch(bf, c, inputs, lf);
        });
    b->setInputCallbacks(
        [this](uint8_t /*gen*/, uint32_t bf, uint8_t c,
               uint8_t const* inputs, uint32_t lf) {
          a->injectRemoteBatch(bf, c, inputs, lf);
        });
    a->focus();
    b->focus();
  }

  // No-op now that send delivers directly; kept so the test reads as a
  // tick loop even though there's nothing queued.
  void deliverAll() {}
};

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

}  // namespace

TEST_CASE("Zero jitter: predictions never trigger", "[rollback][prediction]") {
  auto [common, settings] = makeEnv();
  Loopback<RollbackController> rb(common, settings, 0xBEEF);

  // Pre-seed the 3-frame input-delay window.
  for (uint32_t i = 0; i < 3; ++i) {
    rb.a->injectRemoteInput(i, 0);
    rb.b->injectRemoteInput(i, 0);
  }

  Rand inputRng(0xC0FFEE);
  constexpr int kTicks = 500;
  for (int tick = 0; tick < kTicks; ++tick) {
    uint8_t inA = inputRng() & 0x7f;
    uint8_t inB = inputRng() & 0x7f;
    if ((inputRng() % 10) < 6) inA |= (1 << Worm::Fire);
    if ((inputRng() % 10) < 6) inB |= (1 << Worm::Fire);
    rb.a->setLocalControlState(inA);
    rb.b->setLocalControlState(inB);

    rb.a->process();
    rb.b->process();
    rb.deliverAll();
  }

  // confirmedFrame should track simFrame-1 exactly under zero jitter.
  REQUIRE(rb.a->confirmedFrame() ==
          static_cast<int32_t>(rb.a->currentFrame()) - 1);
  REQUIRE(rb.b->confirmedFrame() ==
          static_cast<int32_t>(rb.b->currentFrame()) - 1);

  // Every resident slot is Confirmed.
  auto const& bufA = rb.a->rollbackBuffer();
  for (int f = bufA.oldestFrame(); f <= bufA.newestFrame(); ++f) {
    auto* slot = const_cast<rollback::RollbackBuffer&>(bufA).find(f);
    REQUIRE(slot != nullptr);
    REQUIRE(slot->remoteState == rollback::RemoteState::Confirmed);
  }
}

TEST_CASE("Starvation: predict up to kMaxRollback, then stall, then recover",
          "[rollback][prediction]") {
  auto [common, settings] = makeEnv();
  // Drive A directly via injectRemoteInput; B is unused. No transport.
  RollbackController a(common, settings, 0);
  a.setSkipWeaponSelection(true);
  a.game.rand.seed(0x1234);
  a.focus();

  // Seed remote inputs for exactly the warm-up frames so A advances in
  // confirmed lockstep up to kWarm and no further.
  constexpr uint32_t kWarm = 20;
  for (uint32_t f = 0; f < kWarm; ++f) {
    a.injectRemoteInput(f, 0);
  }
  for (uint32_t i = 0; i < kWarm; ++i) {
    a.setLocalControlState(0);
    a.process();
  }
  REQUIRE(a.currentFrame() == kWarm);
  REQUIRE(a.confirmedFrame() == static_cast<int32_t>(kWarm) - 1);

  // Stop delivering remote inputs. A predicts up to kMaxRollback frames
  // ahead and then stalls (subsequent process() calls become no-ops on
  // simFrame).
  constexpr int kStarveTicks = rollback::kMaxRollback + 5;
  uint32_t frameAtStarveStart = a.currentFrame();
  for (int i = 0; i < kStarveTicks; ++i) {
    a.setLocalControlState(0);
    a.process();
  }
  // Bound: simFrame advanced by at most kMaxRollback past confirmedFrame.
  REQUIRE(a.currentFrame() == frameAtStarveStart + rollback::kMaxRollback);
  REQUIRE(a.confirmedFrame() == static_cast<int32_t>(frameAtStarveStart) - 1);

  // The predicted frames are marked Predicted in the buffer.
  auto const& buf = a.rollbackBuffer();
  int predictedCount = 0, confirmedCount = 0;
  for (int f = buf.oldestFrame(); f <= buf.newestFrame(); ++f) {
    auto* slot = const_cast<rollback::RollbackBuffer&>(buf).find(f);
    REQUIRE(slot != nullptr);
    if (slot->remoteState == rollback::RemoteState::Predicted)
      ++predictedCount;
    else
      ++confirmedCount;
  }
  REQUIRE(predictedCount == rollback::kMaxRollback);
  // Buffer holds kMaxRollback+1 slots total → 1 confirmed straggler.
  REQUIRE(confirmedCount == 1);

  // Late delivery: inject the previously-missing remote inputs. The
  // promote step on the next process() should catch confirmedFrame up
  // to simFrame-1 (we sent idle 0, predicted idle 0 — they match in
  // *input* terms).
  uint32_t simFrameAtStall = a.currentFrame();
  for (uint32_t f = frameAtStarveStart; f < simFrameAtStall; ++f) {
    a.injectRemoteInput(f, 0);
  }
  // One more process() drains the late-arriving inputs via the promote
  // step. Then a second tick can actually advance again because we keep
  // delivering input-delay frames (we also inject inputs for the next
  // few frames so the new frame can be Confirmed-advanced).
  for (uint32_t f = simFrameAtStall; f < simFrameAtStall + 3; ++f) {
    a.injectRemoteInput(f, 0);
  }
  a.setLocalControlState(0);
  a.process();

  REQUIRE(a.confirmedFrame() >= static_cast<int32_t>(simFrameAtStall) - 1);
  REQUIRE(a.currentFrame() > simFrameAtStall);
}
