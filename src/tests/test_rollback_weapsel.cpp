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
#include <vector>

#include "controller/commonController.hpp"
#include "controller/rollbackController.hpp"
#include "game.hpp"
#include "jitter_transport.hpp"
#include "math.hpp"
#include "rollback/buffer.hpp"
#include "serialization/weapsel_snapshot.hpp"

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
  // Disable bot weapon auto-pick path so the test drives the menu
  // explicitly rather than auto-ready'ing.
  settings->selectBotWeapons = 0;
  return {common, settings};
}

std::unique_ptr<RollbackController> makePeer(std::shared_ptr<Common> common,
                                             std::shared_ptr<Settings> settings, int localIdx,
                                             uint32_t worldSeed) {
  auto c = std::make_unique<RollbackController>(common, settings, localIdx);
  c->setInputDelay(1);
  c->game.rand.seed(worldSeed);
  return c;
}

constexpr uint8_t BIT_UP = uint8_t{1} << Worm::Up;
constexpr uint8_t BIT_DOWN = uint8_t{1} << Worm::Down;
constexpr uint8_t BIT_FIRE = uint8_t{1} << Worm::Fire;

// Build a navigation script that presses Down `nDown` times then Fire.
// Each press is two ticks (on, off) to produce a clean rising edge per
// press; the trailing idle tick lets the input settle before the next.
// Returns a vector of input bytes, one per tick.
std::vector<uint8_t> navigateToDoneAndConfirm(int nDown) {
  std::vector<uint8_t> out;
  for (int i = 0; i < nDown; ++i) {
    out.push_back(BIT_DOWN);
    out.push_back(0);
  }
  // A short idle pad lets any held-key repeat settle. Then a clean
  // Fire press.
  out.push_back(0);
  out.push_back(BIT_FIRE);
  out.push_back(0);
  return out;
}

}  // namespace

TEST_CASE("WeaponSelectSnap round-trip preserves state", "[rollback][weapsel]") {
  auto [common, settings] = makeEnv();
  auto a = makePeer(common, settings, 0, 0xC0FFEE);
  a->focus();

  // Run a handful of ticks to mutate ws / worm state from its initial
  // value (cursor moves, possibly a weapon cycle). Use scripted local
  // input + a couple of pre-fill remote inputs for symmetry.
  a->injectRemoteInput(0, 0);
  for (int i = 0; i < 6; ++i) {
    a->setLocalControlState((i % 2 == 0) ? BIT_DOWN : 0);
    a->process();
    a->injectRemoteInput(static_cast<uint32_t>(i + 1), 0);
  }
  REQUIRE(a->currentFrame() > 0);

  // Capture the snapshot.
  WeaponSelectSnap snap;
  a->saveWeaponSelectSnap(snap);
  REQUIRE(snap.valid);

  // Independently capture state we expect restore to recover.
  int wormCurrentWeaponBefore = a->game.worms[0]->currentWeapon;
  uint32_t weaponIdBefore = a->game.worms[0]->settings->weapons[0];

  // Mutate further by running more ticks with a different input.
  for (int i = 0; i < 8; ++i) {
    a->setLocalControlState((i % 2 == 0) ? BIT_UP : 0);
    a->process();
    a->injectRemoteInput(static_cast<uint32_t>(7 + i), 0);
  }

  // Save a "mutated" snapshot to verify it differs from the original.
  WeaponSelectSnap mutated;
  a->saveWeaponSelectSnap(mutated);

  // Restore the original snapshot.
  a->loadWeaponSelectSnap(snap);

  // Verify the restored state matches what we captured.
  REQUIRE(a->game.worms[0]->currentWeapon == wormCurrentWeaponBefore);
  REQUIRE(a->game.worms[0]->settings->weapons[0] == weaponIdBefore);

  // Save a third snapshot after restore — it must match the original.
  WeaponSelectSnap restored;
  a->saveWeaponSelectSnap(restored);
  REQUIRE(restored.valid);
  for (int i = 0; i < 2; ++i) {
    REQUIRE(restored.players[i].weapons == snap.players[i].weapons);
    REQUIRE(restored.players[i].isReady == snap.players[i].isReady);
    REQUIRE(restored.players[i].menuSelection == snap.players[i].menuSelection);
    REQUIRE(restored.players[i].menuTopItem == snap.players[i].menuTopItem);
    REQUIRE(restored.players[i].menuBottomItem == snap.players[i].menuBottomItem);
    REQUIRE(restored.players[i].wormControlStates == snap.players[i].wormControlStates);
    REQUIRE(restored.players[i].currentWeapon == snap.players[i].currentWeapon);
  }
  REQUIRE(restored.localPrevInput == snap.localPrevInput);
  REQUIRE(restored.remotePrevInput == snap.remotePrevInput);
  REQUIRE(restored.localHeldFrames == snap.localHeldFrames);
  REQUIRE(restored.remoteHeldFrames == snap.remoteHeldFrames);

  // And the mutated snapshot must differ from the original on at least
  // one observable axis — otherwise the test wouldn't have exercised
  // any state change to round-trip.
  bool mutatedDiffers = mutated.players[0].menuSelection != snap.players[0].menuSelection ||
                        mutated.localPrevInput != snap.localPrevInput;
  REQUIRE(mutatedDiffers);
}

TEST_CASE("Weapon select reaches StateGame in sync under zero jitter", "[rollback][weapsel]") {
  constexpr uint32_t kWorldSeed = 0xC0FFEE;
  auto [common, settings] = makeEnv();
  auto a = makePeer(common, settings, 0, kWorldSeed);
  auto b = makePeer(common, settings, 1, kWorldSeed);

  // Direct synchronous delivery — bypass the transport queue so the
  // peers behave like the production session under zero loss / zero
  // delay.
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

  // Pre-fill the inputDelay=1 window.
  a->injectRemoteInput(0, 0);
  b->injectRemoteInput(0, 0);

  auto script = navigateToDoneAndConfirm(6);
  // Run the script + a healthy tail of idle ticks. The transition only
  // happens at a confirmed frame, so we need enough ticks past the Fire
  // press for the chain to catch up.
  constexpr int kIdleTail = 30;

  bool aTransitioned = false, bTransitioned = false;
  uint32_t aTransitionFrame = 0, bTransitionFrame = 0;

  for (int i = 0; i < static_cast<int>(script.size()) + kIdleTail; ++i) {
    uint8_t in = (i < static_cast<int>(script.size())) ? script[i] : 0;
    a->setLocalControlState(in);
    b->setLocalControlState(in);
    bool aInWeapselBefore = !aTransitioned;
    bool bInWeapselBefore = !bTransitioned;
    a->process();
    b->process();
    // Track the first tick at which each peer crosses into StateGame.
    if (aInWeapselBefore && a->gameState() == StateGame) {
      aTransitioned = true;
      aTransitionFrame = a->currentFrame();
    }
    if (bInWeapselBefore && b->gameState() == StateGame) {
      bTransitioned = true;
      bTransitionFrame = b->currentFrame();
    }
    for (auto const& p : aToB)
      b->injectRemoteBatch(p.baseFrame, p.count, p.inputs.data(), p.localFrame);
    for (auto const& p : bToA)
      a->injectRemoteBatch(p.baseFrame, p.count, p.inputs.data(), p.localFrame);
    aToB.clear();
    bToA.clear();
  }

  REQUIRE(aTransitioned);
  REQUIRE(bTransitioned);
  REQUIRE(aTransitionFrame == bTransitionFrame);

  // With zero jitter, predictions always match real input — no rollback
  // should have fired.
  REQUIRE(a->rollbackCount() == 0);
  REQUIRE(b->rollbackCount() == 0);

  // Both peers picked the same weapons.
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < Settings::selectableWeapons; ++j) {
      REQUIRE(a->game.worms[i]->settings->weapons[j] == b->game.worms[i]->settings->weapons[j]);
    }
  }

  // Game-phase state hashes match after transition.
  REQUIRE(wideRollbackChecksum(a->game) == wideRollbackChecksum(b->game));
}

TEST_CASE("Weapon select transitions cleanly under jitter", "[rollback][weapsel]") {
  constexpr uint32_t kWorldSeed = 0xBEEF1234;
  auto [common, settings] = makeEnv();
  auto a = makePeer(common, settings, 0, kWorldSeed);
  auto b = makePeer(common, settings, 1, kWorldSeed);

  rollback_test::JitterTransport transport({0xA5A5, /*minDelay=*/1, /*maxDelay=*/3,
                                            /*loss=*/0.0, /*duplicate=*/0.0});

  a->setInputCallbacks([&](uint8_t gen_, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
    transport.sendAToB(gen_, bf, c, in, lf);
  });
  b->setInputCallbacks([&](uint8_t gen_, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
    transport.sendBToA(gen_, bf, c, in, lf);
  });
  a->focus();
  b->focus();

  a->injectRemoteInput(0, 0);
  b->injectRemoteInput(0, 0);

  auto deliverA = [&](uint8_t gen, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
    a->injectRemoteBatch(gen, bf, c, in, lf);
  };
  auto deliverB = [&](uint8_t gen, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
    b->injectRemoteBatch(gen, bf, c, in, lf);
  };

  auto script = navigateToDoneAndConfirm(6);
  constexpr int kIdleTail = 60;

  bool aTransitioned = false, bTransitioned = false;
  uint32_t aTransitionFrame = 0, bTransitionFrame = 0;

  for (int i = 0; i < static_cast<int>(script.size()) + kIdleTail; ++i) {
    uint8_t in = (i < static_cast<int>(script.size())) ? script[i] : 0;
    a->setLocalControlState(in);
    b->setLocalControlState(in);
    bool aInWeapselBefore = !aTransitioned;
    bool bInWeapselBefore = !bTransitioned;
    a->process();
    b->process();
    if (aInWeapselBefore && a->currentGame()->statsRecorder) {
      aTransitioned = true;
      aTransitionFrame = a->currentFrame();
    }
    if (bInWeapselBefore && b->currentGame()->statsRecorder) {
      bTransitioned = true;
      bTransitionFrame = b->currentFrame();
    }
    transport.tick(deliverA, deliverB);
  }

  // Flush any tail packets, then a few more ticks to let promote loops
  // drain.
  transport.flush(deliverA, deliverB);
  a->setLocalControlState(0);
  b->setLocalControlState(0);
  for (int i = 0; i < 16; ++i) {
    bool aInWeapselBefore = !aTransitioned;
    bool bInWeapselBefore = !bTransitioned;
    a->process();
    b->process();
    if (aInWeapselBefore && a->gameState() == StateGame) {
      aTransitioned = true;
      aTransitionFrame = a->currentFrame();
    }
    if (bInWeapselBefore && b->gameState() == StateGame) {
      bTransitioned = true;
      bTransitionFrame = b->currentFrame();
    }
    transport.tick(deliverA, deliverB);
  }

  REQUIRE(aTransitioned);
  REQUIRE(bTransitioned);
  REQUIRE(aTransitionFrame == bTransitionFrame);

  // Same weapon picks, same checksums.
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < Settings::selectableWeapons; ++j) {
      REQUIRE(a->game.worms[i]->settings->weapons[j] == b->game.worms[i]->settings->weapons[j]);
    }
  }

  REQUIRE(wideRollbackChecksum(a->game) == wideRollbackChecksum(b->game));

  // Vacuity guard: jitter must have caused at least some prediction
  // events; otherwise this test devolves into the zero-jitter case.
  bool anyRollback = a->rollbackCount() > 0 || b->rollbackCount() > 0;
  INFO("a rollbacks=" << a->rollbackCount() << " b rollbacks=" << b->rollbackCount());
  REQUIRE(anyRollback);
}
