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
#include <climits>
#include <cstdint>
#include <memory>

#include "controller/rollbackController.hpp"
#include "game.hpp"
#include "gfx.hpp"
#include "math.hpp"
#include "mixer/player.hpp"
#include "rollback/buffer.hpp"
#include "weapon.hpp"

namespace {

// IsPlaying always answers false so the sim's sound-dependent branches
// (loop-sound retrigger checks) behave identically across controllers,
// keeping play counts comparable between runs.
struct CountingSoundPlayer : SoundPlayer {
  int plays = 0;
  bool IsPlaying(void* /*id*/) override { return false; }
  void Stop(void* /*id*/) override {}

 protected:
  void PlayImpl(int /*sound*/, void* /*id*/, int /*loops*/) override { ++plays; }
};

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
      : common(std::move(common_in)),
        settings(std::move(settings_in)),
        a(std::make_unique<Ctrl>(common, settings, 0)),
        b(std::make_unique<Ctrl>(common, settings, 1)) {
    a->SetSkipWeaponSelection(/*skip=*/true);
    b->SetSkipWeaponSelection(/*skip=*/true);
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
    if ((input_rng() % 10) < 6) {
      in_a |= (1 << Worm::kFire);
    }
    if ((input_rng() % 10) < 6) {
      in_b |= (1 << Worm::kFire);
    }
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
  a.SetSkipWeaponSelection(/*skip=*/true);
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
  uint32_t const kFrameAtStarveStart = a.CurrentFrame();
  for (int i = 0; i < kStarveTicks; ++i) {
    a.SetLocalControlState(0);
    a.Process();
  }
  // Bound: simFrame advanced by at most kMaxRollback past confirmedFrame.
  REQUIRE(a.CurrentFrame() == kFrameAtStarveStart + rollback::kMaxRollback);
  REQUIRE(a.ConfirmedFrame() == static_cast<int32_t>(kFrameAtStarveStart) - 1);

  // The predicted frames are marked Predicted in the buffer.
  auto const& buf = a.RollbackBuffer();
  int predicted_count = 0;
  int confirmed_count = 0;
  for (int f = buf.OldestFrame(); f <= buf.NewestFrame(); ++f) {
    auto* slot = const_cast<rollback::RollbackBuffer&>(buf).Find(f);
    REQUIRE(slot != nullptr);
    if (slot->remote_state == rollback::RemoteState::kPredicted) {
      ++predicted_count;
    } else {
      ++confirmed_count;
    }
  }
  REQUIRE(predicted_count == rollback::kMaxRollback);
  // Buffer holds kMaxRollback+1 slots total → 1 confirmed straggler.
  REQUIRE(confirmed_count == 1);

  // Late delivery: inject the previously-missing remote inputs. The
  // promote step on the next process() should catch confirmedFrame up
  // to simFrame-1 (we sent idle 0, predicted idle 0 — they match in
  // *input* terms).
  uint32_t const kSimFrameAtStall = a.CurrentFrame();
  for (uint32_t f = kFrameAtStarveStart; f < kSimFrameAtStall; ++f) {
    a.InjectRemoteInput(f, 0);
  }
  // One more process() drains the late-arriving inputs via the promote
  // step. Then a second tick can actually advance again because we keep
  // delivering input-delay frames (we also inject inputs for the next
  // few frames so the new frame can be Confirmed-advanced).
  for (uint32_t f = kSimFrameAtStall; f < kSimFrameAtStall + 3; ++f) {
    a.InjectRemoteInput(f, 0);
  }
  a.SetLocalControlState(0);
  a.Process();

  REQUIRE(a.ConfirmedFrame() >= static_cast<int32_t>(kSimFrameAtStall) - 1);
  REQUIRE(a.CurrentFrame() > kSimFrameAtStall);
}

TEST_CASE("Predicted frames emit sound and promote does not re-emit",
          "[rollback][prediction][sound]") {
  // Regression for the netplay silence bug: frames first executed as
  // *predicted* used to run speculatively, so their sounds were never
  // emitted — and a correct prediction is later promoted without
  // re-execution, so they never got a second chance. Under real
  // latency that muted essentially the whole match.
  //
  // The remote worm does all the playing: its input is a pure function
  // of the frame index (so subject and control see identical input
  // streams), toggling Fire for spawn edges, then holding Fire from
  // well before the starve window through the end so predictions are
  // always correct and every frame is executed exactly once. The
  // subject is starved (frames run predicted, inputs delivered late);
  // the control gets every input on time. Their final play counts
  // must match exactly: predicted execution emits, promotion does not
  // duplicate.
  auto [common, settings] = MakeEnv();

  constexpr uint8_t kFireBit = (1 << Worm::kFire);
  // Long enough for the remote worm to spawn and settle into firing:
  // the respawn camera scroll alone takes up to ~160 frames.
  constexpr uint32_t kWarm = 500;
  constexpr uint32_t kHoldFrom = kWarm - 60;
  constexpr uint32_t kFinal = kWarm + rollback::kMaxRollback + 30;

  auto remote_in = [](uint32_t f) -> uint8_t {
    if (f >= kHoldFrom) {
      return kFireBit;  // constant across the prediction window
    }
    return (f % 24) < 16 ? kFireBit : 0;  // edges so the worm spawns
  };

  // Give every worm slot the fastest-firing weapon with a launch sound
  // so shots land inside the kMaxRollback-frame prediction window.
  int best_pos = -1;
  int best_delay = INT_MAX;
  for (std::size_t p = 0; p < common->weap_order.size(); ++p) {
    Weapon const& w = common->weapons[common->weap_order[p]];
    if (w.launch_sound >= 0 && w.ammo >= 10 && w.delay < best_delay) {
      best_delay = w.delay;
      best_pos = static_cast<int>(p);
    }
  }
  REQUIRE(best_pos >= 0);
  REQUIRE(best_delay < rollback::kMaxRollback);
  for (int const kWs : {0, 1, Settings::kNetworkPlayerIdx}) {
    for (uint32_t& w : settings->worm_settings[kWs]->weapons) {
      w = static_cast<uint32_t>(best_pos) + 1;
    }
  }

  auto setup = [](RollbackController& c, std::shared_ptr<CountingSoundPlayer> const& sp) {
    c.SetSkipWeaponSelection(/*skip=*/true);
    c.game.rand.Seed(0x5EED);
    c.game.sound_player = sp;
    c.Focus();
  };

  auto sp_sub = std::make_shared<CountingSoundPlayer>();
  RollbackController sub(common, settings, 0);
  setup(sub, sp_sub);

  // Warm-up: remote input present, frames run confirmed. Inject
  // per-tick — the input ring is small, injecting the whole range up
  // front would wrap it.
  for (uint32_t i = 0; i < kWarm; ++i) {
    sub.InjectRemoteInput(i, remote_in(i));
    sub.SetLocalControlState(0);
    sub.Process();
  }
  REQUIRE(sub.CurrentFrame() == kWarm);
  int const kPlaysWarm = sp_sub->plays;
  REQUIRE(kPlaysWarm > 1);

  // Starve: every executed frame is predicted. They must still sound.
  for (int i = 0; i < rollback::kMaxRollback + 5; ++i) {
    sub.SetLocalControlState(0);
    sub.Process();
  }
  REQUIRE(sub.CurrentFrame() == kWarm + rollback::kMaxRollback);
  REQUIRE(sp_sub->plays > kPlaysWarm);

  // Late delivery of the (matching) remote inputs lifts the stall via
  // the promote path; keep feeding inputs and run to kFinal.
  for (uint32_t f = kWarm; f < kFinal + 8; ++f) {
    sub.InjectRemoteInput(f, remote_in(f));
  }
  for (int guard = 0; sub.CurrentFrame() < kFinal && guard < 1000; ++guard) {
    sub.SetLocalControlState(0);
    sub.Process();
  }
  REQUIRE(sub.CurrentFrame() == kFinal);

  // Control: identical seed and inputs, never starved.
  auto sp_ctl = std::make_shared<CountingSoundPlayer>();
  RollbackController ctl(common, settings, 0);
  setup(ctl, sp_ctl);
  for (uint32_t i = 0; i < kFinal; ++i) {
    ctl.InjectRemoteInput(i, remote_in(i));
    ctl.SetLocalControlState(0);
    ctl.Process();
  }
  REQUIRE(ctl.CurrentFrame() == kFinal);

  REQUIRE(sp_sub->plays == sp_ctl->plays);
}

TEST_CASE("Shadow game does not hijack g_soundPlayer", "[rollback][sound]") {
  // Regression: the shadow game's ctor used to install its
  // NullSoundPlayer as g_soundPlayer, muting every menu/UI sound for
  // the rest of the match — and leaving a dangling global after a
  // rematch cycle.
  auto [common, settings] = MakeEnv();

  RollbackController a(common, settings, 0);
  a.SetSkipWeaponSelection(/*skip=*/true);
  a.game.rand.Seed(0x1234);
  a.Focus();

  REQUIRE(a.ShadowGameForTest() != nullptr);
  // The live game's (gfx-shared) player must still be the global one.
  REQUIRE(g_sound_player == gfx.sound_player.get());
}
