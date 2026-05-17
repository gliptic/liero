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

  gvl::mwc inputRng(12345);

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
  REQUIRE(f.controllerA->game.rand.x == f.controllerB->game.rand.x);
  REQUIRE(f.controllerA->game.rand.c == f.controllerB->game.rand.c);
}
