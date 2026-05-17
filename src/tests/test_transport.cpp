#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <thread>
#include <chrono>

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

  // We need to get the actual port. ENet doesn't expose this easily,
  // so let's use a fixed port for testing.
  host.~NetTransport();
  new (&host) NetTransport();

  uint16_t port = 19532;
  REQUIRE(host.host(port));
  REQUIRE(host.state() == NetTransport::Listening);

  bool hostConnected = false;
  bool clientConnected = false;
  host.onConnected = [&]() { hostConnected = true; };

  NetTransport client;
  client.onConnected = [&]() { clientConnected = true; };
  REQUIRE(client.connect("127.0.0.1", port));

  // Poll both until connected
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while ((!hostConnected || !clientConnected) &&
         std::chrono::steady_clock::now() < deadline) {
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
  uint16_t port = 19533;

  NetTransport host;
  REQUIRE(host.host(port));

  NetTransport client;
  REQUIRE(client.connect("127.0.0.1", port));

  // Wait for connection
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while ((host.state() != NetTransport::Connected ||
          client.state() != NetTransport::Connected) &&
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
  while (receivedFrame == 0xFFFFFFFF &&
         std::chrono::steady_clock::now() < deadline) {
    host.poll();
    client.poll();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  REQUIRE(receivedFrame == 42);
  REQUIRE(receivedInput == 0x55);
}

TEST_CASE("Transport delivers handshake", "[transport]") {
  uint16_t port = 19534;

  NetTransport host;
  REQUIRE(host.host(port));

  NetTransport client;
  REQUIRE(client.connect("127.0.0.1", port));

  // Wait for connection
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while ((host.state() != NetTransport::Connected ||
          client.state() != NetTransport::Connected) &&
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

TEST_CASE("Transport bidirectional input exchange", "[transport]") {
  uint16_t port = 19535;

  NetTransport host;
  REQUIRE(host.host(port));

  NetTransport client;
  REQUIRE(client.connect("127.0.0.1", port));

  // Wait for connection
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while ((host.state() != NetTransport::Connected ||
          client.state() != NetTransport::Connected) &&
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
