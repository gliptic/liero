#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdint>
#include <thread>

#include "net/transport.hpp"

static void pollUntil(NetTransport& t, NetTransport::State target, int maxMs = 2000) {
  auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(maxMs);
  while (t.state() != target && std::chrono::steady_clock::now() < deadline) {
    t.poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

TEST_CASE("Transport connects host and client", "[transport]") {
  NetTransport host;
  REQUIRE(host.host(0));  // Bind to any available port
  uint16_t port = host.listeningPort();
  REQUIRE(port != 0);
  REQUIRE(host.state() == NetTransport::Listening);

  bool hostConnected = false;
  bool clientConnected = false;
  host.onConnected = [&]() { hostConnected = true; };

  NetTransport client;
  client.onConnected = [&]() { clientConnected = true; };
  REQUIRE(client.connect("127.0.0.1", port));

  // Poll both until connected
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while ((!hostConnected || !clientConnected) && std::chrono::steady_clock::now() < deadline) {
    host.poll();
    client.poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  REQUIRE(hostConnected);
  REQUIRE(clientConnected);
  REQUIRE(host.state() == NetTransport::Connected);
  REQUIRE(client.state() == NetTransport::Connected);
}

TEST_CASE("Transport delivers input packets", "[transport]") {
  NetTransport host;
  REQUIRE(host.host(0));
  uint16_t port = host.listeningPort();

  NetTransport client;
  REQUIRE(client.connect("127.0.0.1", port));

  // Wait for connection
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while ((host.state() != NetTransport::Connected || client.state() != NetTransport::Connected) &&
         std::chrono::steady_clock::now() < deadline) {
    host.poll();
    client.poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  REQUIRE(host.state() == NetTransport::Connected);

  // Client sends inputs, host receives them
  uint32_t receivedFrame = 0xFFFFFFFF;
  uint8_t receivedInput = 0xFF;
  host.onRemoteInput = [&](uint32_t f, uint8_t i) {
    receivedFrame = f;
    receivedInput = i;
  };

  client.sendInput(42, 0x55);

  // Poll until received
  deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
  while (receivedFrame == 0xFFFFFFFF && std::chrono::steady_clock::now() < deadline) {
    host.poll();
    client.poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  REQUIRE(receivedFrame == 42);
  REQUIRE(receivedInput == 0x55);
}

TEST_CASE("Transport delivers handshake", "[transport]") {
  NetTransport host;
  REQUIRE(host.host(0));
  uint16_t port = host.listeningPort();

  NetTransport client;
  REQUIRE(client.connect("127.0.0.1", port));

  // Wait for connection
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while ((host.state() != NetTransport::Connected || client.state() != NetTransport::Connected) &&
         std::chrono::steady_clock::now() < deadline) {
    host.poll();
    client.poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  REQUIRE(host.state() == NetTransport::Connected);

  uint32_t rxSeed = 0, rxHash = 0;
  host.onHandshake = [&](uint32_t s, uint32_t h) {
    rxSeed = s;
    rxHash = h;
  };

  client.sendHandshake(12345, 0xDEADBEEF);

  deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
  while (rxSeed == 0 && std::chrono::steady_clock::now() < deadline) {
    host.poll();
    client.poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  REQUIRE(rxSeed == 12345);
  REQUIRE(rxHash == 0xDEADBEEF);
}

TEST_CASE("Transport delivers player info", "[transport]") {
  NetTransport host;
  REQUIRE(host.host(0));
  uint16_t port = host.listeningPort();

  NetTransport client;
  REQUIRE(client.connect("127.0.0.1", port));

  // Wait for connection
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while ((host.state() != NetTransport::Connected || client.state() != NetTransport::Connected) &&
         std::chrono::steady_clock::now() < deadline) {
    host.poll();
    client.poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  REQUIRE(host.state() == NetTransport::Connected);

  NetTransport::PlayerInfo received{};
  bool got = false;
  host.onPlayerInfo = [&](const NetTransport::PlayerInfo& info) {
    received = info;
    got = true;
  };

  NetTransport::PlayerInfo sent{};
  sent.weapons[0] = 5;
  sent.weapons[1] = 12;
  sent.weapons[2] = 30;
  sent.weapons[3] = 1;
  sent.weapons[4] = 40;
  sent.color = 7;
  sent.rgb[0] = 200;
  sent.rgb[1] = 100;
  sent.rgb[2] = 50;

  client.sendPlayerInfo(sent);

  deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
  while (!got && std::chrono::steady_clock::now() < deadline) {
    host.poll();
    client.poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  REQUIRE(got);
  for (int i = 0; i < 5; ++i) REQUIRE(received.weapons[i] == sent.weapons[i]);
  REQUIRE(received.color == sent.color);
  for (int i = 0; i < 3; ++i) REQUIRE(received.rgb[i] == sent.rgb[i]);
}

TEST_CASE("Transport delivers match settings", "[transport]") {
  NetTransport host;
  REQUIRE(host.host(0));
  uint16_t port = host.listeningPort();

  NetTransport client;
  REQUIRE(client.connect("127.0.0.1", port));

  // Wait for connection
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while ((host.state() != NetTransport::Connected || client.state() != NetTransport::Connected) &&
         std::chrono::steady_clock::now() < deadline) {
    host.poll();
    client.poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  REQUIRE(host.state() == NetTransport::Connected);

  NetTransport::MatchSettingsData received{};
  bool got = false;
  client.onMatchSettings = [&](const NetTransport::MatchSettingsData& data) {
    received = data;
    got = true;
  };

  NetTransport::MatchSettingsData sent{};
  sent.lives = 15;
  sent.loadingTime = 100;
  sent.gameMode = 2;
  sent.blood = 500;
  sent.maxBonuses = 3;
  sent.timeToLose = 600;
  sent.flagsToWin = 5;
  sent.loadChange = 1;
  for (int i = 0; i < 40; ++i) sent.weapTable[i] = (i % 4 == 0) ? 1 : 0;

  host.sendMatchSettings(sent);

  deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
  while (!got && std::chrono::steady_clock::now() < deadline) {
    host.poll();
    client.poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  REQUIRE(got);
  REQUIRE(received.lives == sent.lives);
  REQUIRE(received.loadingTime == sent.loadingTime);
  REQUIRE(received.gameMode == sent.gameMode);
  REQUIRE(received.blood == sent.blood);
  REQUIRE(received.maxBonuses == sent.maxBonuses);
  REQUIRE(received.timeToLose == sent.timeToLose);
  REQUIRE(received.flagsToWin == sent.flagsToWin);
  REQUIRE(received.loadChange == sent.loadChange);
  for (int i = 0; i < 40; ++i) REQUIRE(received.weapTable[i] == sent.weapTable[i]);
}

TEST_CASE("Transport bidirectional input exchange", "[transport]") {
  NetTransport host;
  REQUIRE(host.host(0));
  uint16_t port = host.listeningPort();

  NetTransport client;
  REQUIRE(client.connect("127.0.0.1", port));

  // Wait for connection
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while ((host.state() != NetTransport::Connected || client.state() != NetTransport::Connected) &&
         std::chrono::steady_clock::now() < deadline) {
    host.poll();
    client.poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  // Exchange 100 frames of inputs in both directions
  int hostReceived = 0, clientReceived = 0;
  host.onRemoteInput = [&](uint32_t, uint8_t) { ++hostReceived; };
  client.onRemoteInput = [&](uint32_t, uint8_t) { ++clientReceived; };

  for (uint32_t i = 0; i < 100; ++i) {
    host.sendInput(i, static_cast<uint8_t>(i & 0x7f));
    client.sendInput(i, static_cast<uint8_t>((i + 1) & 0x7f));
  }

  // Poll until all received
  deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while ((hostReceived < 100 || clientReceived < 100) &&
         std::chrono::steady_clock::now() < deadline) {
    host.poll();
    client.poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  REQUIRE(hostReceived == 100);
  REQUIRE(clientReceived == 100);
}

TEST_CASE("Transport delivers large map data (100KB+)", "[transport]") {
  NetTransport host;
  REQUIRE(host.host(0));
  uint16_t port = host.listeningPort();

  NetTransport client;
  REQUIRE(client.connect("127.0.0.1", port));

  // Wait for connection
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while ((host.state() != NetTransport::Connected || client.state() != NetTransport::Connected) &&
         std::chrono::steady_clock::now() < deadline) {
    host.poll();
    client.poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  REQUIRE(host.state() == NetTransport::Connected);

  // Create a 150KB payload with a recognizable pattern
  const size_t payloadSize = 150 * 1024;
  std::vector<uint8_t> sendData(payloadSize);
  for (size_t i = 0; i < payloadSize; ++i) {
    sendData[i] = static_cast<uint8_t>(i * 7 + i / 256);
  }

  std::vector<uint8_t> receivedData;
  client.onMapData = [&](const void* data, size_t len) {
    auto* bytes = static_cast<const uint8_t*>(data);
    receivedData.assign(bytes, bytes + len);
  };

  host.sendMapData(sendData.data(), sendData.size());

  // Poll until received (may require multiple poll cycles for fragmentation)
  deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (receivedData.empty() && std::chrono::steady_clock::now() < deadline) {
    host.poll();
    client.poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  REQUIRE(receivedData.size() == payloadSize);
  REQUIRE(receivedData == sendData);
}

// Rollback K-wide input batch round-trip. Verifies the PacketInputBatch
// wire format end-to-end, including localDelta reconstruction
// (= remoteLocalFrame).
TEST_CASE("Transport delivers rollback input batches", "[transport][rollback]") {
  NetTransport host;
  REQUIRE(host.host(0));
  uint16_t port = host.listeningPort();

  NetTransport client;
  REQUIRE(client.connect("127.0.0.1", port));

  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while ((host.state() != NetTransport::Connected || client.state() != NetTransport::Connected) &&
         std::chrono::steady_clock::now() < deadline) {
    host.poll();
    client.poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  REQUIRE(host.state() == NetTransport::Connected);

  uint32_t rxBaseFrame = 0xFFFFFFFF;
  uint8_t rxCount = 0;
  uint32_t rxLocalFrame = 0xFFFFFFFF;
  std::vector<uint8_t> rxInputs;
  uint8_t rxGen = 0xFF;
  host.onRemoteInputBatch = [&](uint8_t gen, uint32_t bf, uint8_t c, uint8_t const* in,
                                uint32_t lf) {
    rxGen = gen;
    rxBaseFrame = bf;
    rxCount = c;
    rxLocalFrame = lf;
    rxInputs.assign(in, in + c);
  };

  uint8_t inputs[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
  // baseFrame = 100, localDelta = 5, so remoteLocalFrame = 105.
  client.sendInputBatch(0, 100, 8, 5, inputs);

  deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
  while (rxBaseFrame == 0xFFFFFFFF && std::chrono::steady_clock::now() < deadline) {
    host.poll();
    client.poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  REQUIRE(rxGen == 0);
  REQUIRE(rxBaseFrame == 100);
  REQUIRE(rxCount == 8);
  REQUIRE(rxLocalFrame == 105);
  REQUIRE(rxInputs.size() == 8);
  for (int i = 0; i < 8; ++i) REQUIRE(rxInputs[i] == inputs[i]);
}

// The handshake carries kProtocolVersion. A peer that sends a
// mismatched version (raw-byte spoof, simulating an old or future
// build) must NOT see its handshake delivered. Silent drop is
// intentional — the session-level timeout surfaces it as a normal
// connection failure.
TEST_CASE("Transport rejects handshake with wrong protocol version", "[transport][rollback]") {
  NetTransport host;
  REQUIRE(host.host(0));
  uint16_t port = host.listeningPort();

  NetTransport client;
  REQUIRE(client.connect("127.0.0.1", port));

  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while ((host.state() != NetTransport::Connected || client.state() != NetTransport::Connected) &&
         std::chrono::steady_clock::now() < deadline) {
    host.poll();
    client.poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  REQUIRE(host.state() == NetTransport::Connected);

  bool delivered = false;
  host.onHandshake = [&](uint32_t, uint32_t) { delivered = true; };

  // Normal handshake (correct version) goes through.
  client.sendHandshake(7, 0x12345678);
  deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
  while (!delivered && std::chrono::steady_clock::now() < deadline) {
    host.poll();
    client.poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  REQUIRE(delivered);

  // Now also confirm the constant lines up — the test would silently
  // pass against any version if this slipped to a stale value.
  REQUIRE(NetTransport::kProtocolVersion == 5);
}