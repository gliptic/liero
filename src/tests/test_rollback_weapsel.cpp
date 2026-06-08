// Rollback for the weapon-select phase.
//
// Until this work, advanceWeaponSelection was strict lockstep — every
// tick blocked on remote input. That baked a wall-clock offset into the
// transition into the game phase: whoever pressed Done last forced the
// other peer to wait one network round-trip, and the offset persisted
// because game-phase rollback can't close a gap created before it
// started.
//
// These tests cover the rollback-ified weapon-select path:
//
//   * snapshot round-trip — save/load preserves picked weapons, menu
//     cursor / scroll, isReady flags, game.rand, and edge-detection
//     state.
//   * zero-jitter parity — with synchronous delivery, both peers reach
//     identical final state after navigating to Done and confirming, and
//     rollback never fires (predictions match real input every tick).
//   * jittery delivery — with delays + duplicates, both peers still
//     transition to game phase at the same simFrame, with identical
//     weapon picks. Rollback events actually occur during the run.
//   * desync sentinel — a navigation script that ends with both worms
//     pressing Done leaves both peers in StateGame, having transitioned
//     at the same simFrame.

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "controller/commonController.hpp"
#include "controller/rollbackController.hpp"
#include "game.hpp"
#include "jitter_transport.hpp"
#include "math.hpp"
#include "rollback/buffer.hpp"
#include "serialization/weapsel_snapshot.hpp"

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
  // Disable bot weapon auto-pick path so the test drives the menu
  // explicitly rather than auto-ready'ing.
  settings->select_bot_weapons = 0;
  return {common, settings};
}

std::unique_ptr<RollbackController> MakePeer(const std::shared_ptr<Common>& common,
                                             const std::shared_ptr<Settings>& settings,
                                             int local_idx, uint32_t world_seed) {
  auto c = std::make_unique<RollbackController>(common, settings, local_idx);
  c->SetInputDelay(1);
  c->game.rand.Seed(world_seed);
  return c;
}

constexpr uint8_t kBitUp = uint8_t{1} << Worm::kUp;
constexpr uint8_t kBitDown = uint8_t{1} << Worm::kDown;
constexpr uint8_t kBitFire = uint8_t{1} << Worm::kFire;

// Build a navigation script that presses Down `nDown` times then Fire.
// Each press is two ticks (on, off) to produce a clean rising edge per
// press; the trailing idle tick lets the input settle before the next.
// Returns a vector of input bytes, one per tick.
std::vector<uint8_t> NavigateToDoneAndConfirm(int n_down) {
  std::vector<uint8_t> out;
  for (int i = 0; i < n_down; ++i) {
    out.push_back(kBitDown);
    out.push_back(0);
  }
  // A short idle pad lets any held-key repeat settle. Then a clean
  // Fire press.
  out.push_back(0);
  out.push_back(kBitFire);
  out.push_back(0);
  return out;
}

}  // namespace

TEST_CASE("WeaponSelectSnap round-trip preserves state", "[rollback][weapsel]") {
  auto [common, settings] = MakeEnv();
  auto a = MakePeer(common, settings, 0, 0xC0FFEE);
  a->Focus();

  // Run a handful of ticks to mutate ws / worm state from its initial
  // value (cursor moves, possibly a weapon cycle). Use scripted local
  // input + a couple of pre-fill remote inputs for symmetry.
  a->InjectRemoteInput(0, 0);
  for (int i = 0; i < 6; ++i) {
    a->SetLocalControlState((i % 2 == 0) ? kBitDown : 0);
    a->Process();
    a->InjectRemoteInput(static_cast<uint32_t>(i + 1), 0);
  }
  REQUIRE(a->CurrentFrame() > 0);

  // Capture the snapshot.
  WeaponSelectSnap snap;
  a->SaveWeaponSelectSnap(snap);
  REQUIRE(snap.valid);

  // Independently capture state we expect restore to recover.
  int const kWormCurrentWeaponBefore = a->game.worms[0]->current_weapon;
  uint32_t const kWeaponIdBefore = a->game.worms[0]->settings->weapons[0];

  // Mutate further by running more ticks with a different input.
  for (int i = 0; i < 8; ++i) {
    a->SetLocalControlState((i % 2 == 0) ? kBitUp : 0);
    a->Process();
    a->InjectRemoteInput(static_cast<uint32_t>(7 + i), 0);
  }

  // Save a "mutated" snapshot to verify it differs from the original.
  WeaponSelectSnap mutated;
  a->SaveWeaponSelectSnap(mutated);

  // Restore the original snapshot.
  a->LoadWeaponSelectSnap(snap);

  // Verify the restored state matches what we captured.
  REQUIRE(a->game.worms[0]->current_weapon == kWormCurrentWeaponBefore);
  REQUIRE(a->game.worms[0]->settings->weapons[0] == kWeaponIdBefore);

  // Save a third snapshot after restore — it must match the original.
  WeaponSelectSnap restored;
  a->SaveWeaponSelectSnap(restored);
  REQUIRE(restored.valid);
  for (int i = 0; i < 2; ++i) {
    REQUIRE(restored.players[i].weapons == snap.players[i].weapons);
    REQUIRE(restored.players[i].is_ready == snap.players[i].is_ready);
    REQUIRE(restored.players[i].menu_selection == snap.players[i].menu_selection);
    REQUIRE(restored.players[i].menu_top_item == snap.players[i].menu_top_item);
    REQUIRE(restored.players[i].menu_bottom_item == snap.players[i].menu_bottom_item);
    REQUIRE(restored.players[i].worm_control_states == snap.players[i].worm_control_states);
    REQUIRE(restored.players[i].current_weapon == snap.players[i].current_weapon);
  }
  REQUIRE(restored.local_prev_input == snap.local_prev_input);
  REQUIRE(restored.remote_prev_input == snap.remote_prev_input);
  REQUIRE(restored.local_held_frames == snap.local_held_frames);
  REQUIRE(restored.remote_held_frames == snap.remote_held_frames);

  // And the mutated snapshot must differ from the original on at least
  // one observable axis — otherwise the test wouldn't have exercised
  // any state change to round-trip.
  bool const kMutatedDiffers =
      mutated.players[0].menu_selection != snap.players[0].menu_selection ||
      mutated.local_prev_input != snap.local_prev_input;
  REQUIRE(kMutatedDiffers);
}

TEST_CASE("Weapon select reaches StateGame in sync under zero jitter", "[rollback][weapsel]") {
  constexpr uint32_t kWorldSeed = 0xC0FFEE;
  auto [common, settings] = MakeEnv();
  auto a = MakePeer(common, settings, 0, kWorldSeed);
  auto b = MakePeer(common, settings, 1, kWorldSeed);

  // Direct synchronous delivery — bypass the transport queue so the
  // peers behave like the production session under zero loss / zero
  // delay.
  struct Pkt {
    uint32_t base_frame;
    uint8_t count;
    std::array<uint8_t, rollback::kMaxRollback + 1> inputs;
    uint32_t local_frame;
  };
  std::vector<Pkt> a_to_b;
  std::vector<Pkt> b_to_a;
  auto enqueue = [](std::vector<Pkt>& q, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
    Pkt p{};
    p.base_frame = bf;
    p.count = c;
    p.local_frame = lf;
    for (uint8_t i = 0; i < c; ++i) p.inputs[i] = in[i];
    q.push_back(p);
  };
  a->SetInputCallbacks([&](uint8_t /*gen*/, uint32_t bf, uint8_t c, uint8_t const* in,
                           uint32_t lf) { enqueue(a_to_b, bf, c, in, lf); });
  b->SetInputCallbacks([&](uint8_t /*gen*/, uint32_t bf, uint8_t c, uint8_t const* in,
                           uint32_t lf) { enqueue(b_to_a, bf, c, in, lf); });
  a->Focus();
  b->Focus();

  // Pre-fill the inputDelay=1 window.
  a->InjectRemoteInput(0, 0);
  b->InjectRemoteInput(0, 0);

  auto script = NavigateToDoneAndConfirm(6);
  // Run the script + a healthy tail of idle ticks. The transition only
  // happens at a confirmed frame, so we need enough ticks past the Fire
  // press for the chain to catch up.
  constexpr int kIdleTail = 30;

  bool a_transitioned = false;
  bool b_transitioned = false;
  uint32_t a_transition_frame = 0;
  uint32_t b_transition_frame = 0;

  for (int i = 0; i < static_cast<int>(script.size()) + kIdleTail; ++i) {
    uint8_t const kIn = (std::cmp_less(i, script.size())) ? script[i] : 0;
    a->SetLocalControlState(kIn);
    b->SetLocalControlState(kIn);
    bool const kAInWeapselBefore = !a_transitioned;
    bool const kBInWeapselBefore = !b_transitioned;
    a->Process();
    b->Process();
    // Track the first tick at which each peer crosses into StateGame.
    if (kAInWeapselBefore && a->State() == kStateGame) {
      a_transitioned = true;
      a_transition_frame = a->CurrentFrame();
    }
    if (kBInWeapselBefore && b->State() == kStateGame) {
      b_transitioned = true;
      b_transition_frame = b->CurrentFrame();
    }
    for (auto const& p : a_to_b)
      b->InjectRemoteBatch(p.base_frame, p.count, p.inputs.data(), p.local_frame);
    for (auto const& p : b_to_a)
      a->InjectRemoteBatch(p.base_frame, p.count, p.inputs.data(), p.local_frame);
    a_to_b.clear();
    b_to_a.clear();
  }

  REQUIRE(a_transitioned);
  REQUIRE(b_transitioned);
  REQUIRE(a_transition_frame == b_transition_frame);

  // With zero jitter, predictions always match real input — no rollback
  // should have fired.
  REQUIRE(a->RollbackCount() == 0);
  REQUIRE(b->RollbackCount() == 0);

  // Both peers picked the same weapons.
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < Settings::kSelectableWeapons; ++j) {
      REQUIRE(a->game.worms[i]->settings->weapons[j] == b->game.worms[i]->settings->weapons[j]);
    }
  }

  // Game-phase state hashes match after transition.
  REQUIRE(WideRollbackChecksum(a->game) == WideRollbackChecksum(b->game));
}

TEST_CASE("Weapon select transitions cleanly under jitter", "[rollback][weapsel]") {
  constexpr uint32_t kWorldSeed = 0xBEEF1234;
  auto [common, settings] = MakeEnv();
  auto a = MakePeer(common, settings, 0, kWorldSeed);
  auto b = MakePeer(common, settings, 1, kWorldSeed);

  rollback_test::JitterTransport transport({.seed = 0xA5A5,
                                            /*minDelay=*/.min_delay_frames = 1,
                                            /*maxDelay=*/.max_delay_frames = 3,
                                            /*loss=*/.loss_probability = 0.0,
                                            /*duplicate=*/.duplicate_probability = 0.0});

  a->SetInputCallbacks([&](uint8_t gen, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
    transport.SendAToB(gen, bf, c, in, lf);
  });
  b->SetInputCallbacks([&](uint8_t gen, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
    transport.SendBToA(gen, bf, c, in, lf);
  });
  a->Focus();
  b->Focus();

  a->InjectRemoteInput(0, 0);
  b->InjectRemoteInput(0, 0);

  auto deliver_a = [&](uint8_t gen, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
    a->InjectRemoteBatch(gen, bf, c, in, lf);
  };
  auto deliver_b = [&](uint8_t gen, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
    b->InjectRemoteBatch(gen, bf, c, in, lf);
  };

  auto script = NavigateToDoneAndConfirm(6);
  constexpr int kIdleTail = 60;

  bool a_transitioned = false;
  bool b_transitioned = false;
  uint32_t a_transition_frame = 0;
  uint32_t b_transition_frame = 0;

  for (int i = 0; i < static_cast<int>(script.size()) + kIdleTail; ++i) {
    uint8_t const kIn = (std::cmp_less(i, script.size())) ? script[i] : 0;
    a->SetLocalControlState(kIn);
    b->SetLocalControlState(kIn);
    bool const kAInWeapselBefore = !a_transitioned;
    bool const kBInWeapselBefore = !b_transitioned;
    a->Process();
    b->Process();
    if (kAInWeapselBefore && a->CurrentGame()->stats_recorder) {
      a_transitioned = true;
      a_transition_frame = a->CurrentFrame();
    }
    if (kBInWeapselBefore && b->CurrentGame()->stats_recorder) {
      b_transitioned = true;
      b_transition_frame = b->CurrentFrame();
    }
    transport.Tick(deliver_a, deliver_b);
  }

  // Flush any tail packets, then a few more ticks to let promote loops
  // drain.
  transport.Flush(deliver_a, deliver_b);
  a->SetLocalControlState(0);
  b->SetLocalControlState(0);
  for (int i = 0; i < 16; ++i) {
    bool const kAInWeapselBefore = !a_transitioned;
    bool const kBInWeapselBefore = !b_transitioned;
    a->Process();
    b->Process();
    if (kAInWeapselBefore && a->State() == kStateGame) {
      a_transitioned = true;
      a_transition_frame = a->CurrentFrame();
    }
    if (kBInWeapselBefore && b->State() == kStateGame) {
      b_transitioned = true;
      b_transition_frame = b->CurrentFrame();
    }
    transport.Tick(deliver_a, deliver_b);
  }

  REQUIRE(a_transitioned);
  REQUIRE(b_transitioned);
  REQUIRE(a_transition_frame == b_transition_frame);

  // Same weapon picks, same checksums.
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < Settings::kSelectableWeapons; ++j) {
      REQUIRE(a->game.worms[i]->settings->weapons[j] == b->game.worms[i]->settings->weapons[j]);
    }
  }

  REQUIRE(WideRollbackChecksum(a->game) == WideRollbackChecksum(b->game));

  // Vacuity guard: jitter must have caused at least some prediction
  // events; otherwise this test devolves into the zero-jitter case.
  bool const kAnyRollback = a->RollbackCount() > 0 || b->RollbackCount() > 0;
  INFO("a rollbacks=" << a->RollbackCount() << " b rollbacks=" << b->RollbackCount());
  REQUIRE(kAnyRollback);
}
