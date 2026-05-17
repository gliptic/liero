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

  REQUIRE(host.hostGame(0));
  uint16_t port = host.transport().listeningPort();
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
  settingsB->lives = 99;
  settingsB->blood = 200;
  settingsB->loadingTime = 50;
  settingsB->gameMode = Settings::GMGameOfTag;
  settingsB->maxBonuses = 7;
  settingsB->timeToLose = 999;
  settingsB->flagsToWin = 3;
  settingsB->loadChange = true;
  // Modify some weapTable entries
  for (int i = 0; i < 40; ++i)
    settingsB->weapTable[i] = (i < 10) ? 2 : 0;

  NetSession host(f.common, f.settings);
  NetSession client(f.common, settingsB);

  REQUIRE(host.hostGame(0));
  uint16_t port = host.transport().listeningPort();
  REQUIRE(client.joinGame("127.0.0.1", port));

  // Poll until both reach Playing — host settings are authoritative
  bool ready = pollUntil(host, client, [&]() {
    return host.sessionState() == NetSession::Playing &&
           client.sessionState() == NetSession::Playing;
  });

  REQUIRE(ready);
  // Client should have received and applied host's settings
  REQUIRE(settingsB->lives == f.settings->lives);
  REQUIRE(settingsB->blood == f.settings->blood);
  REQUIRE(settingsB->loadingTime == f.settings->loadingTime);
  REQUIRE(settingsB->gameMode == f.settings->gameMode);
  REQUIRE(settingsB->maxBonuses == f.settings->maxBonuses);
  REQUIRE(settingsB->timeToLose == f.settings->timeToLose);
  REQUIRE(settingsB->flagsToWin == f.settings->flagsToWin);
  REQUIRE(settingsB->loadChange == f.settings->loadChange);
  for (int i = 0; i < 40; ++i)
    REQUIRE(settingsB->weapTable[i] == f.settings->weapTable[i]);
}

TEST_CASE("NetSession syncs worm colors and weapons between peers", "[session]") {
  SessionFixture f;

  // Give each player distinct colors and weapons in the network player slot
  auto settingsHost = std::make_shared<Settings>(*f.settings);
  // Create distinct WormSettings objects so shared_ptr sharing doesn't cause issues
  for (int i = 0; i < Settings::NumWormSettings; ++i)
    settingsHost->wormSettings[i] = std::make_shared<WormSettings>(*settingsHost->wormSettings[i]);
  settingsHost->wormSettings[Settings::NetworkPlayerIdx]->color = 3;
  settingsHost->wormSettings[Settings::NetworkPlayerIdx]->rgb[0] = 255;
  settingsHost->wormSettings[Settings::NetworkPlayerIdx]->rgb[1] = 0;
  settingsHost->wormSettings[Settings::NetworkPlayerIdx]->rgb[2] = 0;
  settingsHost->wormSettings[Settings::NetworkPlayerIdx]->weapons[0] = 10;
  settingsHost->wormSettings[Settings::NetworkPlayerIdx]->weapons[1] = 20;
  settingsHost->wormSettings[Settings::NetworkPlayerIdx]->weapons[2] = 30;
  settingsHost->wormSettings[Settings::NetworkPlayerIdx]->weapons[3] = 5;
  settingsHost->wormSettings[Settings::NetworkPlayerIdx]->weapons[4] = 15;

  auto settingsClient = std::make_shared<Settings>(*f.settings);
  for (int i = 0; i < Settings::NumWormSettings; ++i)
    settingsClient->wormSettings[i] = std::make_shared<WormSettings>(*settingsClient->wormSettings[i]);
  settingsClient->wormSettings[Settings::NetworkPlayerIdx]->color = 6;
  settingsClient->wormSettings[Settings::NetworkPlayerIdx]->rgb[0] = 0;
  settingsClient->wormSettings[Settings::NetworkPlayerIdx]->rgb[1] = 255;
  settingsClient->wormSettings[Settings::NetworkPlayerIdx]->rgb[2] = 128;
  settingsClient->wormSettings[Settings::NetworkPlayerIdx]->weapons[0] = 35;
  settingsClient->wormSettings[Settings::NetworkPlayerIdx]->weapons[1] = 8;
  settingsClient->wormSettings[Settings::NetworkPlayerIdx]->weapons[2] = 22;
  settingsClient->wormSettings[Settings::NetworkPlayerIdx]->weapons[3] = 14;
  settingsClient->wormSettings[Settings::NetworkPlayerIdx]->weapons[4] = 40;

  NetSession host(f.common, settingsHost);
  NetSession client(f.common, settingsClient);

  REQUIRE(host.hostGame(0));
  uint16_t port = host.transport().listeningPort();
  REQUIRE(client.joinGame("127.0.0.1", port));

  bool ready = pollUntil(host, client, [&]() {
    return host.sessionState() == NetSession::Playing &&
           client.sessionState() == NetSession::Playing;
  });
  REQUIRE(ready);

  // Host's remote worm (index 1) should have client's color/weapons
  Worm* hostRemoteWorm = host.controller()->game.wormByIdx(1);
  REQUIRE(hostRemoteWorm->settings->color == 6);
  REQUIRE(hostRemoteWorm->settings->rgb[0] == 0);
  REQUIRE(hostRemoteWorm->settings->rgb[1] == 255);
  REQUIRE(hostRemoteWorm->settings->rgb[2] == 128);
  REQUIRE(hostRemoteWorm->settings->weapons[0] == 35);
  REQUIRE(hostRemoteWorm->settings->weapons[4] == 40);

  // Client's remote worm (index 0) should have host's color/weapons
  Worm* clientRemoteWorm = client.controller()->game.wormByIdx(0);
  REQUIRE(clientRemoteWorm->settings->color == 3);
  REQUIRE(clientRemoteWorm->settings->rgb[0] == 255);
  REQUIRE(clientRemoteWorm->settings->rgb[1] == 0);
  REQUIRE(clientRemoteWorm->settings->rgb[2] == 0);
  REQUIRE(clientRemoteWorm->settings->weapons[0] == 10);
  REQUIRE(clientRemoteWorm->settings->weapons[4] == 15);

  // Persistent settings should NOT be modified
  REQUIRE(settingsHost->wormSettings[1]->color != 6);
  REQUIRE(settingsClient->wormSettings[0]->color != 3);
}

TEST_CASE("NetSession plays frames over real network", "[session]") {
  SessionFixture f;

  NetSession host(f.common, f.settings);
  NetSession client(f.common, f.settings);

  REQUIRE(host.hostGame(0));
  uint16_t port = host.transport().listeningPort();
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

TEST_CASE("NetSession client detects host disconnect", "[session]") {
  SessionFixture f;

  NetSession host(f.common, f.settings);
  NetSession client(f.common, f.settings);

  REQUIRE(host.hostGame(0));
  uint16_t port = host.transport().listeningPort();
  REQUIRE(client.joinGame("127.0.0.1", port));

  bool ready = pollUntil(host, client, [&]() {
    return host.sessionState() == NetSession::Playing &&
           client.sessionState() == NetSession::Playing;
  });
  REQUIRE(ready);

  // Host disconnects
  host.disconnect();
  REQUIRE(host.sessionState() == NetSession::Idle);

  // Client should detect the disconnect
  bool disconnected = false;
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (!disconnected && std::chrono::steady_clock::now() < deadline) {
    client.update();
    disconnected = (client.sessionState() == NetSession::Disconnected);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  REQUIRE(disconnected);
}

TEST_CASE("NetSession connect to non-existent host fails", "[session]") {
  SessionFixture f;

  NetSession client(f.common, f.settings);
  // Connect to a port where nobody is listening
  REQUIRE(client.joinGame("127.0.0.1", 19599));

  // Poll — should eventually fail or stay in WaitingForPeer
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (client.sessionState() == NetSession::WaitingForPeer &&
         std::chrono::steady_clock::now() < deadline) {
    client.update();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  // Should not be in Playing state
  REQUIRE(client.sessionState() != NetSession::Playing);
}
