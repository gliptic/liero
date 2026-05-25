#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <memory>
#include <queue>

#include "controller/networkController.hpp"
#include "game.hpp"
#include "math.hpp"
#include "mixer/player.hpp"

// Simulates two NetworkControllers connected back-to-back in-process.
// Each controller's send callback pushes into the other's receive queue.
struct LoopbackFixture {
  std::shared_ptr<Common> common;
  std::shared_ptr<Settings> settings;
  std::unique_ptr<NetworkController> controllerA;  // local player 0
  std::unique_ptr<NetworkController> controllerB;  // local player 1

  // Queues simulating the network: frame -> input
  std::queue<std::pair<uint32_t, uint8_t>> aToB;
  std::queue<std::pair<uint32_t, uint8_t>> bToA;

  LoopbackFixture() {
    precomputeTables();

    common = std::make_shared<Common>();
    FsNode tcRoot(FsNode("data") / "TC" / "openliero");
    common->load(std::move(tcRoot));

    settings = std::make_shared<Settings>();
    settings->lives = 10;
    settings->loadingTime = 0;
    settings->loadChange = true;
    settings->randomLevel = true;
    settings->gameMode = Settings::GMKillEmAll;

    controllerA = std::make_unique<NetworkController>(common, settings, 0);
    controllerB = std::make_unique<NetworkController>(common, settings, 1);

    // Skip weapon selection in tests — we're testing game simulation determinism
    controllerA->setSkipWeaponSelection(true);
    controllerB->setSkipWeaponSelection(true);

    // Seed both games identically
    uint32_t seed = 42;
    controllerA->game.rand.seed(seed);
    controllerB->game.rand.seed(seed);

    // Wire up loopback: A's send goes to B's receive queue and vice versa
    controllerA->setInputCallbacks(
        [this](uint32_t frame, uint8_t input) {
          aToB.push({frame, input});
        },
        [this](uint32_t frame) -> int {
          if (!bToA.empty() && bToA.front().first == frame) {
            int val = bToA.front().second;
            bToA.pop();
            return val;
          }
          return -1;
        });

    controllerB->setInputCallbacks(
        [this](uint32_t frame, uint8_t input) {
          bToA.push({frame, input});
        },
        [this](uint32_t frame) -> int {
          if (!aToB.empty() && aToB.front().first == frame) {
            int val = aToB.front().second;
            aToB.pop();
            return val;
          }
          return -1;
        });

    // Focus both (triggers game initialization)
    controllerA->focus();
    controllerB->focus();
  }

  // Deliver all pending messages (simulate instant delivery)
  void deliverAll() {
    while (!aToB.empty()) {
      auto [frame, input] = aToB.front();
      aToB.pop();
      controllerB->injectRemoteInput(frame, input);
    }
    while (!bToA.empty()) {
      auto [frame, input] = bToA.front();
      bToA.pop();
      controllerA->injectRemoteInput(frame, input);
    }
  }
};

TEST_CASE("NetworkController advances with loopback", "[network]") {
  LoopbackFixture f;

  // Pre-inject initial inputs so both sides can advance
  // With input delay of 3, we need frames 0,1,2 buffered before frame 0 can run
  for (uint32_t i = 0; i < 3; ++i) {
    f.controllerA->injectRemoteInput(i, 0);
    f.controllerB->injectRemoteInput(i, 0);
  }

  // Run several frames
  for (int tick = 0; tick < 100; ++tick) {
    f.controllerA->process();
    f.controllerB->process();
    f.deliverAll();
  }

  // Both should have advanced
  REQUIRE(f.controllerA->currentFrame() > 0);
  REQUIRE(f.controllerA->currentFrame() == f.controllerB->currentFrame());
}

TEST_CASE("NetworkController stalls without remote input", "[network]") {
  LoopbackFixture f;

  // Don't inject any remote input — both should stall at frame 0
  f.controllerA->process();
  f.controllerB->process();

  REQUIRE(f.controllerA->currentFrame() == 0);
  REQUIRE(f.controllerB->currentFrame() == 0);
}

TEST_CASE("NetworkController produces deterministic state", "[network]") {
  LoopbackFixture f;

  Rand inputRng(12345);

  // Pre-seed input delay frames
  for (uint32_t i = 0; i < 3; ++i) {
    uint8_t inputA = inputRng() & 0x7f;
    uint8_t inputB = inputRng() & 0x7f;
    f.controllerA->injectRemoteInput(i, inputB);
    f.controllerB->injectRemoteInput(i, inputA);
    // Also set local inputs for the first few frames
    f.controllerA->game.worms[0]->controlStates.unpack(inputA);
    f.controllerB->game.worms[1]->controlStates.unpack(inputB);
  }

  for (int tick = 0; tick < 200; ++tick) {
    f.controllerA->process();
    f.controllerB->process();
    f.deliverAll();
  }

  // Both games should be at the same frame with identical RNG state
  REQUIRE(f.controllerA->currentFrame() == f.controllerB->currentFrame());
  REQUIRE(f.controllerA->game.rand == f.controllerB->game.rand);
}

TEST_CASE("NetworkController pressedOnce fires only on rising edge", "[network]") {
  // Standalone test with a single controller to precisely control inputs
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

  auto ctrl = std::make_unique<NetworkController>(common, settings, 0);
  ctrl->setSkipWeaponSelection(true);
  ctrl->game.rand.seed(42);

  // Use null send callback (we don't need loopback for this test)
  ctrl->setInputCallbacks([](uint32_t, uint8_t) {}, nullptr);

  ctrl->focus();

  // Pre-fill input delay frames (frames 0-2)
  for (uint32_t i = 0; i < 3; ++i)
    ctrl->injectRemoteInput(i, 0);

  // Run frames until worm 0 becomes visible.
  // killedTimer starts at 150. After it expires, worm needs Fire to become ready.
  // We'll inject Fire at the right time.
  uint8_t fireInput = (1 << Worm::Fire);
  for (int tick = 0; tick < 200; ++tick) {
    uint32_t nextFrame = ctrl->currentFrame();
    // Inject remote input: Fire at frame 150-155 to trigger ready, otherwise empty
    uint8_t remoteIn = (nextFrame >= 150 && nextFrame <= 155) ? fireInput : 0;
    ctrl->injectRemoteInput(nextFrame, remoteIn);
    ctrl->process();
  }

  // Worm 1 (remote) should be visible by now
  REQUIRE(ctrl->game.worms[1]->visible == true);

  // Record initial weapon
  int initialWeapon = ctrl->game.worms[1]->currentWeapon;

  // Now test: inject Change only for 1 frame, then Change+Left for several frames
  uint8_t changeOnly = (1 << Worm::Change);
  uint8_t changeLeft = (1 << Worm::Change) | (1 << Worm::Left);

  uint32_t frame = ctrl->currentFrame();
  ctrl->injectRemoteInput(frame, changeOnly);
  ctrl->process();

  // Next 10 frames: Change+Left held (within initial delay — should fire once)
  for (int tick = 0; tick < 10; ++tick) {
    frame = ctrl->currentFrame();
    ctrl->injectRemoteInput(frame, changeLeft);
    ctrl->process();
  }

  // Weapon should have changed exactly once during initial delay period
  int weaponAfterFirst = ctrl->game.worms[1]->currentWeapon;
  int expected1 = (initialWeapon - 1 + 5) % 5;
  INFO("After initial press: " << weaponAfterFirst << ", expected: " << expected1);
  REQUIRE(weaponAfterFirst == expected1);

  // Continue holding for 20 more frames — should NOT get additional weapon
  // changes (no key repeat in game phase, matching local controller behavior)
  for (int tick = 0; tick < 20; ++tick) {
    frame = ctrl->currentFrame();
    ctrl->injectRemoteInput(frame, changeLeft);
    ctrl->process();
  }

  int weaponAfterHold = ctrl->game.worms[1]->currentWeapon;
  INFO("After holding: " << weaponAfterHold << " (should still be " << expected1 << ")");
  REQUIRE(weaponAfterHold == expected1);
}

TEST_CASE("Weapon selection uses synced game.rand", "[network]") {
  // Two controllers with weapon selection enabled (not skipped).
  // Both have identical worm weapon settings → weapon selection constructor
  // should use game.rand identically on both sides.
  precomputeTables();

  auto common = std::make_shared<Common>();
  FsNode tcRoot(FsNode("data") / "TC" / "openliero");
  common->load(std::move(tcRoot));

  auto settings = std::make_shared<Settings>();
  settings->lives = 10;
  settings->loadingTime = 0;
  settings->randomLevel = true;
  settings->gameMode = Settings::GMKillEmAll;

  // Give all worm profiles the same weapon settings (simulating synced state).
  // In real play, NetSession syncs weapons via PlayerInfo so all three profiles
  // (worm 0, worm 1, NetworkPlayerIdx) end up with consistent values per worm.
  for (int w = 0; w < 3; ++w) {
    for (int i = 0; i < 5; ++i)
      settings->wormSettings[w]->weapons[i] = 5 + i;
  }

  auto ctrlA = std::make_unique<NetworkController>(common, settings, 0);
  auto ctrlB = std::make_unique<NetworkController>(common, settings, 1);

  // Do NOT skip weapon selection
  uint32_t seed = 77;
  ctrlA->game.rand.seed(seed);
  ctrlB->game.rand.seed(seed);

  // Wire loopback
  std::queue<std::pair<uint32_t, uint8_t>> aToB, bToA;
  ctrlA->setInputCallbacks(
      [&](uint32_t frame, uint8_t input) { aToB.push({frame, input}); },
      [&](uint32_t frame) -> int {
        if (!bToA.empty() && bToA.front().first == frame) {
          int v = bToA.front().second; bToA.pop(); return v;
        }
        return -1;
      });
  ctrlB->setInputCallbacks(
      [&](uint32_t frame, uint8_t input) { bToA.push({frame, input}); },
      [&](uint32_t frame) -> int {
        if (!aToB.empty() && aToB.front().first == frame) {
          int v = aToB.front().second; aToB.pop(); return v;
        }
        return -1;
      });

  ctrlA->focus();
  ctrlB->focus();

  // After focus, both should be in weapon selection with identical RNG state
  REQUIRE(ctrlA->game.rand == ctrlB->game.rand);

  // Run a few frames of weapon selection with no input (just idle)
  for (uint32_t i = 0; i < 3; ++i) {
    ctrlA->injectRemoteInput(i, 0);
    ctrlB->injectRemoteInput(i, 0);
  }

  for (int tick = 0; tick < 10; ++tick) {
    ctrlA->process();
    ctrlB->process();
    // Deliver
    while (!aToB.empty()) {
      auto [frame, input] = aToB.front(); aToB.pop();
      ctrlB->injectRemoteInput(frame, input);
    }
    while (!bToA.empty()) {
      auto [frame, input] = bToA.front(); bToA.pop();
      ctrlA->injectRemoteInput(frame, input);
    }
  }

  // RNG should still be identical after weapon selection frames
  REQUIRE(ctrlA->game.rand == ctrlB->game.rand);
}

TEST_CASE("NetworkController determinism fuzz with deaths", "[network][death]") {
  // Full network-layer fuzz: two NetworkControllers wired in loopback,
  // with random inputs delivered through the proper edge detection path.
  // This is the test most likely to reproduce the desync seen in real play.
  LoopbackFixture f;

  // Override to low health for frequent deaths
  for (auto const& w : f.controllerA->game.worms)
    w->health = 30;
  for (auto const& w : f.controllerB->game.worms)
    w->health = 30;
  for (auto const& w : f.controllerA->game.worms)
    w->lives = 30;
  for (auto const& w : f.controllerB->game.worms)
    w->lives = 30;

  // Pre-fill input delay
  for (uint32_t i = 0; i < 3; ++i) {
    f.controllerA->injectRemoteInput(i, 0);
    f.controllerB->injectRemoteInput(i, 0);
  }

  Rand inputRng(0xDEAD);

  constexpr int NUM_TICKS = 10000;
  int deathCount = 0;

  for (int tick = 0; tick < NUM_TICKS; ++tick) {
    // Generate random inputs for both players
    uint8_t inputA = inputRng() & 0x7f;
    uint8_t inputB = inputRng() & 0x7f;

    // Bias toward combat: high chance of fire
    if ((inputRng() % 10) < 6) inputA |= (1 << Worm::Fire);
    if ((inputRng() % 10) < 6) inputB |= (1 << Worm::Fire);

    // Set local control state for each controller.
    // Controller A is player 0: its local input is inputA.
    // Controller B is player 1: its local input is inputB.
    // The edge detection in advanceSimulation() packs localControlState
    // into localInputs[], sends it to the remote, and applies edges.
    f.controllerA->setLocalControlState(inputA);
    f.controllerB->setLocalControlState(inputB);

    f.controllerA->process();
    f.controllerB->process();
    f.deliverAll();

    // Track deaths
    for (auto const& w : f.controllerA->game.worms) {
      if (!w->visible && w->killedTimer == 149)
        ++deathCount;
    }

    // Compare state every frame
    uint32_t frameA = f.controllerA->currentFrame();
    uint32_t frameB = f.controllerB->currentFrame();

    if (frameA == frameB && frameA > 0) {
      uint32_t checksumA = fastGameChecksum(f.controllerA->game);
      uint32_t checksumB = fastGameChecksum(f.controllerB->game);

      if (checksumA != checksumB) {
        INFO("DESYNC at tick " << tick << " (simFrame=" << frameA
             << ", deaths=" << deathCount << ")"
             << "\n  RNG A: x=" << f.controllerA->game.rand.last
             << " c=" << f.controllerA->game.rand.serialize().size()
             << "\n  RNG B: x=" << f.controllerB->game.rand.last
             << " c=" << f.controllerB->game.rand.serialize().size());
        REQUIRE(checksumA == checksumB);
      }
    }

    // Stop if game is over
    if (f.controllerA->game.isGameOver())
      break;
  }

  INFO("Network fuzz completed: " << deathCount << " deaths, "
       << f.controllerA->currentFrame() << " sim frames");
  REQUIRE(deathCount > 0);
}
