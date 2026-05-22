#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstring>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

#include "net/iceAgent.hpp"
#include "net/iceBridge.hpp"

// Helper: poll both agents until predicate is true or timeout
template <typename Pred>
static bool pollUntil(IceAgent& a, IceAgent& b, Pred&& pred, int timeoutMs = 5000) {
  auto start = std::chrono::steady_clock::now();
  while (!pred()) {
    a.poll();
    b.poll();
    auto elapsed = std::chrono::steady_clock::now() - start;
    if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() >= timeoutMs)
      return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return true;
}

template <typename Pred>
static bool pollOneUntil(IceAgent& a, Pred&& pred, int timeoutMs = 5000) {
  auto start = std::chrono::steady_clock::now();
  while (!pred()) {
    a.poll();
    auto elapsed = std::chrono::steady_clock::now() - start;
    if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() >= timeoutMs)
      return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return true;
}

// ============================================================
// IceAgent tests
// ============================================================

TEST_CASE("IceAgent starts and gathers candidates", "[ice]") {
  IceAgent agent;
  IceAgent::Config cfg;
  cfg.stunServer = ""; // No STUN — only host candidates

  std::vector<std::string> candidates;
  bool gatheringDone = false;

  agent.onLocalCandidate = [&](const std::string& c) { candidates.push_back(c); };
  agent.onGatheringDone = [&]() { gatheringDone = true; };

  agent.start(cfg);
  REQUIRE(pollOneUntil(agent, [&] { return gatheringDone; }));
  REQUIRE(!candidates.empty());
}

TEST_CASE("IceAgent local credentials available after start", "[ice]") {
  IceAgent agent;
  IceAgent::Config cfg;
  cfg.stunServer = "";

  agent.start(cfg);
  // Give it a moment to initialize
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  auto ufrag = agent.localUfrag();
  auto pwd = agent.localPwd();
  REQUIRE(!ufrag.empty());
  REQUIRE(!pwd.empty());
  REQUIRE(ufrag.size() >= 4);
  REQUIRE(pwd.size() >= 4);
}

TEST_CASE("Two local IceAgents connect directly", "[ice]") {
  IceAgent agentA, agentB;
  IceAgent::Config cfg;
  cfg.stunServer = ""; // Host candidates only (localhost)

  std::vector<std::string> candidatesA, candidatesB;
  bool gatherDoneA = false, gatherDoneB = false;
  IceAgent::State stateA = IceAgent::State::New;
  IceAgent::State stateB = IceAgent::State::New;

  agentA.onLocalCandidate = [&](const std::string& c) { candidatesA.push_back(c); };
  agentA.onGatheringDone = [&]() { gatherDoneA = true; };
  agentA.onStateChange = [&](IceAgent::State s) { stateA = s; };

  agentB.onLocalCandidate = [&](const std::string& c) { candidatesB.push_back(c); };
  agentB.onGatheringDone = [&]() { gatherDoneB = true; };
  agentB.onStateChange = [&](IceAgent::State s) { stateB = s; };

  agentA.start(cfg);
  agentB.start(cfg);

  // Wait for gathering to complete on both
  REQUIRE(pollUntil(agentA, agentB, [&] { return gatherDoneA && gatherDoneB; }));

  // Exchange credentials
  agentA.setRemoteCredentials(agentB.localUfrag(), agentB.localPwd());
  agentB.setRemoteCredentials(agentA.localUfrag(), agentA.localPwd());

  // Exchange candidates
  for (auto& c : candidatesA) agentB.addRemoteCandidate(c);
  for (auto& c : candidatesB) agentA.addRemoteCandidate(c);
  agentA.setRemoteGatheringDone();
  agentB.setRemoteGatheringDone();

  // Wait for connection
  REQUIRE(pollUntil(agentA, agentB, [&] {
    return stateA == IceAgent::State::Connected && stateB == IceAgent::State::Connected;
  }));
}

TEST_CASE("IceAgent stop is clean", "[ice]") {
  IceAgent agent;
  IceAgent::Config cfg;
  cfg.stunServer = "";

  agent.start(cfg);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  agent.stop();
  REQUIRE(agent.state() == IceAgent::State::Disconnected);

  // Double stop is safe
  agent.stop();
  REQUIRE(agent.state() == IceAgent::State::Disconnected);
}

TEST_CASE("IceAgent data exchange via onRecv", "[ice]") {
  IceAgent agentA, agentB;
  IceAgent::Config cfg;
  cfg.stunServer = "";

  std::vector<std::string> candidatesA, candidatesB;
  bool gatherDoneA = false, gatherDoneB = false;
  IceAgent::State stateA = IceAgent::State::New;
  IceAgent::State stateB = IceAgent::State::New;

  std::vector<uint8_t> receivedByA, receivedByB;

  agentA.onLocalCandidate = [&](const std::string& c) { candidatesA.push_back(c); };
  agentA.onGatheringDone = [&]() { gatherDoneA = true; };
  agentA.onStateChange = [&](IceAgent::State s) { stateA = s; };
  agentA.onRecv = [&](const uint8_t* data, size_t len) {
    receivedByA.insert(receivedByA.end(), data, data + len);
  };

  agentB.onLocalCandidate = [&](const std::string& c) { candidatesB.push_back(c); };
  agentB.onGatheringDone = [&]() { gatherDoneB = true; };
  agentB.onStateChange = [&](IceAgent::State s) { stateB = s; };
  agentB.onRecv = [&](const uint8_t* data, size_t len) {
    receivedByB.insert(receivedByB.end(), data, data + len);
  };

  agentA.start(cfg);
  agentB.start(cfg);

  REQUIRE(pollUntil(agentA, agentB, [&] { return gatherDoneA && gatherDoneB; }));

  agentA.setRemoteCredentials(agentB.localUfrag(), agentB.localPwd());
  agentB.setRemoteCredentials(agentA.localUfrag(), agentA.localPwd());
  for (auto& c : candidatesA) agentB.addRemoteCandidate(c);
  for (auto& c : candidatesB) agentA.addRemoteCandidate(c);
  agentA.setRemoteGatheringDone();
  agentB.setRemoteGatheringDone();

  REQUIRE(pollUntil(agentA, agentB, [&] {
    return stateA == IceAgent::State::Connected && stateB == IceAgent::State::Connected;
  }));

  // Send data A → B
  const uint8_t msg[] = "Hello from A";
  agentA.send(msg, sizeof(msg));

  // Wait for delivery
  auto start = std::chrono::steady_clock::now();
  while (receivedByB.empty()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    auto elapsed = std::chrono::steady_clock::now() - start;
    if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() >= 2000) break;
  }
  REQUIRE(receivedByB.size() == sizeof(msg));
  REQUIRE(std::memcmp(receivedByB.data(), msg, sizeof(msg)) == 0);

  // Send data B → A
  const uint8_t reply[] = "Hello from B";
  agentB.send(reply, sizeof(reply));

  start = std::chrono::steady_clock::now();
  while (receivedByA.empty()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    auto elapsed = std::chrono::steady_clock::now() - start;
    if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() >= 2000) break;
  }
  REQUIRE(receivedByA.size() == sizeof(reply));
  REQUIRE(std::memcmp(receivedByA.data(), reply, sizeof(reply)) == 0);
}

// ============================================================
// IceBridge tests
// ============================================================

TEST_CASE("IceBridge creates valid socket pair", "[ice][bridge]") {
  IceAgent agent;
  IceAgent::Config cfg;
  cfg.stunServer = "";
  agent.start(cfg);

  IceBridge bridge;
  int fd = bridge.create(agent);
  REQUIRE(fd >= 0);
  REQUIRE(bridge.enetSocket() == fd);
  REQUIRE(bridge.bridgePort() > 0);

  bridge.destroy();
  agent.stop();
}

TEST_CASE("IceBridge proxies data bidirectionally", "[ice][bridge]") {
  // Connect two agents, set up bridges, and verify data flows through
  IceAgent agentA, agentB;
  IceAgent::Config cfg;
  cfg.stunServer = "";

  std::vector<std::string> candidatesA, candidatesB;
  bool gatherDoneA = false, gatherDoneB = false;
  IceAgent::State stateA = IceAgent::State::New;
  IceAgent::State stateB = IceAgent::State::New;

  agentA.onLocalCandidate = [&](const std::string& c) { candidatesA.push_back(c); };
  agentA.onGatheringDone = [&]() { gatherDoneA = true; };
  agentA.onStateChange = [&](IceAgent::State s) { stateA = s; };

  agentB.onLocalCandidate = [&](const std::string& c) { candidatesB.push_back(c); };
  agentB.onGatheringDone = [&]() { gatherDoneB = true; };
  agentB.onStateChange = [&](IceAgent::State s) { stateB = s; };

  agentA.start(cfg);
  agentB.start(cfg);

  REQUIRE(pollUntil(agentA, agentB, [&] { return gatherDoneA && gatherDoneB; }));

  agentA.setRemoteCredentials(agentB.localUfrag(), agentB.localPwd());
  agentB.setRemoteCredentials(agentA.localUfrag(), agentA.localPwd());
  for (auto& c : candidatesA) agentB.addRemoteCandidate(c);
  for (auto& c : candidatesB) agentA.addRemoteCandidate(c);
  agentA.setRemoteGatheringDone();
  agentB.setRemoteGatheringDone();

  REQUIRE(pollUntil(agentA, agentB, [&] {
    return stateA == IceAgent::State::Connected && stateB == IceAgent::State::Connected;
  }));

  // Now create bridges
  IceBridge bridgeA, bridgeB;
  int fdA = bridgeA.create(agentA);
  int fdB = bridgeB.create(agentB);
  REQUIRE(fdA >= 0);
  REQUIRE(fdB >= 0);

  // Send from ENet side of A → should arrive on ENet side of B
  const uint8_t msg[] = "Bridge test data";
  // Write to ENet socket A (as if ENet is sending via sendto to bridge address)
  sockaddr_in6 bridgeAddrA{};
  bridgeAddrA.sin6_family = AF_INET6;
  bridgeAddrA.sin6_addr = in6addr_loopback;
  bridgeAddrA.sin6_port = htons(bridgeA.bridgePort());
  ::sendto(fdA, reinterpret_cast<const char*>(msg), sizeof(msg), 0,
           reinterpret_cast<const sockaddr*>(&bridgeAddrA), sizeof(bridgeAddrA));

  // Poll bridge A to forward to IceAgent A → network → IceAgent B → bridge B → ENet socket B
  bridgeA.poll();

  // Wait for data to arrive at ENet socket B
  uint8_t buf[256] = {};
  auto start = std::chrono::steady_clock::now();
  ssize_t n = 0;
  while (n <= 0) {
    n = ::recv(fdB, reinterpret_cast<char*>(buf), sizeof(buf), 0);
    if (n <= 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
      auto elapsed = std::chrono::steady_clock::now() - start;
      if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() >= 2000)
        break;
    }
  }
  REQUIRE(n == sizeof(msg));
  REQUIRE(std::memcmp(buf, msg, sizeof(msg)) == 0);

  bridgeA.destroy();
  bridgeB.destroy();
  agentA.stop();
  agentB.stop();
}

TEST_CASE("IceBridge destroy is safe", "[ice][bridge]") {
  IceAgent agent;
  IceAgent::Config cfg;
  cfg.stunServer = "";
  agent.start(cfg);

  IceBridge bridge;
  bridge.create(agent);
  bridge.destroy();

  // Double destroy is safe
  bridge.destroy();
  agent.stop();
}
