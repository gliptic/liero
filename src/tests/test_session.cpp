#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>

#include "game.hpp"
#include "math.hpp"
#include "net/session.hpp"

struct SessionFixture {
  std::shared_ptr<Common> common;
  std::shared_ptr<Settings> settings;

  SessionFixture() {
    precomputeTables();

    common = std::make_shared<Common>();
    FsNode tcRoot(FsNode("data") / "TC" / "openliero");
    common->load(std::move(tcRoot));

    settings = std::make_shared<Settings>();
    settings->lives = 10;
    settings->loadingTime = 0;
    settings->randomLevel = true;
    settings->gameMode = Settings::GMKillEmAll;
  }
};

// Poll both sessions until a condition is met or timeout
template <typename Pred>
static bool pollUntil(NetSession& a, NetSession& b, Pred pred, int maxMs = 3000) {
  auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(maxMs);
  while (!pred() && std::chrono::steady_clock::now() < deadline) {
    a.update();
    b.update();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return pred();
}

TEST_CASE("NetSession host and client connect and handshake", "[session]") {
  SessionFixture f;

  NetSession host(f.common, f.settings);
  NetSession client(f.common, f.settings);

  uint16_t port = 19540;
  REQUIRE(host.hostGame(port));
  REQUIRE(host.sessionState() == NetSession::WaitingForPeer);

  REQUIRE(client.joinGame("127.0.0.1", port));
  REQUIRE(client.sessionState() == NetSession::WaitingForPeer);

  // Poll until both reach Playing state
  bool ready = pollUntil(host, client, [&]() {
    return host.sessionState() == NetSession::Playing &&
           client.sessionState() == NetSession::Playing;
  });

  REQUIRE(ready);
  REQUIRE(host.controller() != nullptr);
  REQUIRE(client.controller() != nullptr);

  // Both games should have the same RNG seed
  REQUIRE(host.controller()->game.rand.x == client.controller()->game.rand.x);
  REQUIRE(host.controller()->game.rand.c == client.controller()->game.rand.c);
}

TEST_CASE("NetSession syncs host settings to client", "[session]") {
  SessionFixture f;

  auto settingsB = std::make_shared<Settings>(*f.settings);
  settingsB->lives = 99;  // Different from host

  NetSession host(f.common, f.settings);
  NetSession client(f.common, settingsB);

  uint16_t port = 19541;
  REQUIRE(host.hostGame(port));
  REQUIRE(client.joinGame("127.0.0.1", port));

  // Poll until both reach Playing — host settings are authoritative
  bool ready = pollUntil(host, client, [&]() {
    return host.sessionState() == NetSession::Playing &&
           client.sessionState() == NetSession::Playing;
  });

  REQUIRE(ready);
  // Client should have received and applied host's settings
  REQUIRE(settingsB->lives == f.settings->lives);
}

TEST_CASE("NetSession plays frames over real network", "[session]") {
  SessionFixture f;

  NetSession host(f.common, f.settings);
  NetSession client(f.common, f.settings);

  uint16_t port = 19542;
  REQUIRE(host.hostGame(port));
  REQUIRE(client.joinGame("127.0.0.1", port));

  // Wait for Playing state
  bool ready = pollUntil(host, client, [&]() {
    return host.sessionState() == NetSession::Playing &&
           client.sessionState() == NetSession::Playing;
  });
  REQUIRE(ready);

  // Focus both controllers to start the game
  host.controller()->focus();
  client.controller()->focus();

  // Pre-inject remote inputs for the input delay window (3 frames)
  for (uint32_t i = 0; i < 3; ++i) {
    host.controller()->injectRemoteInput(i, 0);
    client.controller()->injectRemoteInput(i, 0);
  }

  // Run 50 frames, polling transport each time
  for (int tick = 0; tick < 50; ++tick) {
    host.controller()->process();
    client.controller()->process();
    host.update();
    client.update();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  // Both should have advanced
  REQUIRE(host.controller()->currentFrame() > 0);
  REQUIRE(host.controller()->currentFrame() == client.controller()->currentFrame());

  // RNG state should be identical (deterministic lockstep)
  REQUIRE(host.controller()->game.rand.x == client.controller()->game.rand.x);
  REQUIRE(host.controller()->game.rand.c == client.controller()->game.rand.c);
}
