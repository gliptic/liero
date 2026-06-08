#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstring>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include "net/iceAgent.hpp"
#include "net/iceBridge.hpp"

// Helper: poll both agents until predicate is true or timeout
template <typename Pred>
static bool PollUntil(IceAgent& a, IceAgent& b, Pred pred, int timeout_ms = 5000) {
  auto start = std::chrono::steady_clock::now();
  while (!pred()) {
    a.Poll();
    b.Poll();
    auto elapsed = std::chrono::steady_clock::now() - start;
    if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() >= timeout_ms)
      return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return true;
}

template <typename Pred>
static bool PollOneUntil(IceAgent& a, Pred pred, int timeout_ms = 5000) {
  auto start = std::chrono::steady_clock::now();
  while (!pred()) {
    a.Poll();
    auto elapsed = std::chrono::steady_clock::now() - start;
    if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() >= timeout_ms)
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
  cfg.stun_server = "";  // No STUN — only host candidates

  std::vector<std::string> candidates;
  bool gathering_done = false;

  agent.on_local_candidate = [&](const std::string& c) { candidates.push_back(c); };
  agent.on_gathering_done = [&]() { gathering_done = true; };

  agent.Start(cfg);
  REQUIRE(PollOneUntil(agent, [&] { return gathering_done; }));
  REQUIRE(!candidates.empty());
}

TEST_CASE("IceAgent local credentials available after start", "[ice]") {
  IceAgent agent;
  IceAgent::Config cfg;
  cfg.stun_server = "";

  agent.Start(cfg);
  // Give it a moment to initialize
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  auto ufrag = agent.LocalUfrag();
  auto pwd = agent.LocalPwd();
  REQUIRE(!ufrag.empty());
  REQUIRE(!pwd.empty());
  REQUIRE(ufrag.size() >= 4);
  REQUIRE(pwd.size() >= 4);
}

TEST_CASE("Two local IceAgents connect directly", "[ice]") {
  IceAgent agent_a;
  IceAgent agent_b;
  IceAgent::Config cfg;
  cfg.stun_server = "";  // Host candidates only (localhost)

  std::vector<std::string> candidates_a;
  std::vector<std::string> candidates_b;
  bool gather_done_a = false;
  bool gather_done_b = false;
  IceAgent::State state_a = IceAgent::State::kNew;
  IceAgent::State state_b = IceAgent::State::kNew;

  agent_a.on_local_candidate = [&](const std::string& c) { candidates_a.push_back(c); };
  agent_a.on_gathering_done = [&]() { gather_done_a = true; };
  agent_a.on_state_change = [&](IceAgent::State s) { state_a = s; };

  agent_b.on_local_candidate = [&](const std::string& c) { candidates_b.push_back(c); };
  agent_b.on_gathering_done = [&]() { gather_done_b = true; };
  agent_b.on_state_change = [&](IceAgent::State s) { state_b = s; };

  agent_a.Start(cfg);
  agent_b.Start(cfg);

  // Wait for gathering to complete on both
  REQUIRE(PollUntil(agent_a, agent_b, [&] { return gather_done_a && gather_done_b; }));

  // Exchange credentials
  agent_a.SetRemoteCredentials(agent_b.LocalUfrag(), agent_b.LocalPwd());
  agent_b.SetRemoteCredentials(agent_a.LocalUfrag(), agent_a.LocalPwd());

  // Exchange candidates
  for (auto& c : candidates_a) agent_b.AddRemoteCandidate(c);
  for (auto& c : candidates_b) agent_a.AddRemoteCandidate(c);
  agent_a.SetRemoteGatheringDone();
  agent_b.SetRemoteGatheringDone();

  // Wait for connection
  REQUIRE(PollUntil(agent_a, agent_b, [&] {
    return state_a == IceAgent::State::kConnected && state_b == IceAgent::State::kConnected;
  }));
}

TEST_CASE("IceAgent stop is clean", "[ice]") {
  IceAgent agent;
  IceAgent::Config cfg;
  cfg.stun_server = "";

  agent.Start(cfg);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  agent.Stop();
  REQUIRE(agent.CurrentState() == IceAgent::State::kDisconnected);

  // Double stop is safe
  agent.Stop();
  REQUIRE(agent.CurrentState() == IceAgent::State::kDisconnected);
}

TEST_CASE("IceAgent data exchange via onRecv", "[ice]") {
  IceAgent agent_a;
  IceAgent agent_b;
  IceAgent::Config cfg;
  cfg.stun_server = "";

  std::vector<std::string> candidates_a;
  std::vector<std::string> candidates_b;
  bool gather_done_a = false;
  bool gather_done_b = false;
  IceAgent::State state_a = IceAgent::State::kNew;
  IceAgent::State state_b = IceAgent::State::kNew;

  std::vector<uint8_t> received_by_a;
  std::vector<uint8_t> received_by_b;

  agent_a.on_local_candidate = [&](const std::string& c) { candidates_a.push_back(c); };
  agent_a.on_gathering_done = [&]() { gather_done_a = true; };
  agent_a.on_state_change = [&](IceAgent::State s) { state_a = s; };
  agent_a.on_recv = [&](const uint8_t* data, size_t len) {
    received_by_a.insert(received_by_a.end(), data, data + len);
  };

  agent_b.on_local_candidate = [&](const std::string& c) { candidates_b.push_back(c); };
  agent_b.on_gathering_done = [&]() { gather_done_b = true; };
  agent_b.on_state_change = [&](IceAgent::State s) { state_b = s; };
  agent_b.on_recv = [&](const uint8_t* data, size_t len) {
    received_by_b.insert(received_by_b.end(), data, data + len);
  };

  agent_a.Start(cfg);
  agent_b.Start(cfg);

  REQUIRE(PollUntil(agent_a, agent_b, [&] { return gather_done_a && gather_done_b; }));

  agent_a.SetRemoteCredentials(agent_b.LocalUfrag(), agent_b.LocalPwd());
  agent_b.SetRemoteCredentials(agent_a.LocalUfrag(), agent_a.LocalPwd());
  for (auto& c : candidates_a) agent_b.AddRemoteCandidate(c);
  for (auto& c : candidates_b) agent_a.AddRemoteCandidate(c);
  agent_a.SetRemoteGatheringDone();
  agent_b.SetRemoteGatheringDone();

  REQUIRE(PollUntil(agent_a, agent_b, [&] {
    return state_a == IceAgent::State::kConnected && state_b == IceAgent::State::kConnected;
  }));

  // Send data A → B
  const uint8_t kMsg[] = "Hello from A";
  agent_a.Send(kMsg, sizeof(kMsg));

  // Wait for delivery
  auto start = std::chrono::steady_clock::now();
  while (received_by_b.empty()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    auto elapsed = std::chrono::steady_clock::now() - start;
    if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() >= 2000) break;
  }
  REQUIRE(received_by_b.size() == sizeof(kMsg));
  REQUIRE(std::memcmp(received_by_b.data(), kMsg, sizeof(kMsg)) == 0);

  // Send data B → A
  const uint8_t kReply[] = "Hello from B";
  agent_b.Send(kReply, sizeof(kReply));

  start = std::chrono::steady_clock::now();
  while (received_by_a.empty()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    auto elapsed = std::chrono::steady_clock::now() - start;
    if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() >= 2000) break;
  }
  REQUIRE(received_by_a.size() == sizeof(kReply));
  REQUIRE(std::memcmp(received_by_a.data(), kReply, sizeof(kReply)) == 0);
}

// ============================================================
// IceBridge tests
// ============================================================

TEST_CASE("IceBridge creates valid socket pair", "[ice][bridge]") {
  IceAgent agent;
  IceAgent::Config cfg;
  cfg.stun_server = "";
  agent.Start(cfg);

  IceBridge bridge;
  int const kFd = bridge.Create(agent);
  REQUIRE(kFd >= 0);
  REQUIRE(bridge.EnetSocket() == kFd);
  REQUIRE(bridge.BridgePort() > 0);

  bridge.Destroy();
  agent.Stop();
}

TEST_CASE("IceBridge proxies data bidirectionally", "[ice][bridge]") {
  // Connect two agents, set up bridges, and verify data flows through
  IceAgent agent_a;
  IceAgent agent_b;
  IceAgent::Config cfg;
  cfg.stun_server = "";

  std::vector<std::string> candidates_a;
  std::vector<std::string> candidates_b;
  bool gather_done_a = false;
  bool gather_done_b = false;
  IceAgent::State state_a = IceAgent::State::kNew;
  IceAgent::State state_b = IceAgent::State::kNew;

  agent_a.on_local_candidate = [&](const std::string& c) { candidates_a.push_back(c); };
  agent_a.on_gathering_done = [&]() { gather_done_a = true; };
  agent_a.on_state_change = [&](IceAgent::State s) { state_a = s; };

  agent_b.on_local_candidate = [&](const std::string& c) { candidates_b.push_back(c); };
  agent_b.on_gathering_done = [&]() { gather_done_b = true; };
  agent_b.on_state_change = [&](IceAgent::State s) { state_b = s; };

  agent_a.Start(cfg);
  agent_b.Start(cfg);

  REQUIRE(PollUntil(agent_a, agent_b, [&] { return gather_done_a && gather_done_b; }));

  agent_a.SetRemoteCredentials(agent_b.LocalUfrag(), agent_b.LocalPwd());
  agent_b.SetRemoteCredentials(agent_a.LocalUfrag(), agent_a.LocalPwd());
  for (auto& c : candidates_a) agent_b.AddRemoteCandidate(c);
  for (auto& c : candidates_b) agent_a.AddRemoteCandidate(c);
  agent_a.SetRemoteGatheringDone();
  agent_b.SetRemoteGatheringDone();

  REQUIRE(PollUntil(agent_a, agent_b, [&] {
    return state_a == IceAgent::State::kConnected && state_b == IceAgent::State::kConnected;
  }));

  // Now create bridges
  IceBridge bridge_a;
  IceBridge bridge_b;
  int const kFdA = bridge_a.Create(agent_a);
  int const kFdB = bridge_b.Create(agent_b);
  REQUIRE(kFdA >= 0);
  REQUIRE(kFdB >= 0);

  // Send from ENet side of A → should arrive on ENet side of B
  const uint8_t kMsg[] = "Bridge test data";
  // Write to ENet socket A (as if ENet is sending via sendto to bridge address)
  sockaddr_in6 bridge_addr_a{};
  bridge_addr_a.sin6_family = AF_INET6;
  bridge_addr_a.sin6_addr = in6addr_loopback;
  bridge_addr_a.sin6_port = htons(bridge_a.BridgePort());
  ::sendto(kFdA, reinterpret_cast<const char*>(kMsg), sizeof(kMsg), 0,
           reinterpret_cast<const sockaddr*>(&bridge_addr_a), sizeof(bridge_addr_a));

  // Poll bridge A to forward to IceAgent A → network → IceAgent B → bridge B → ENet socket B
  bridge_a.Poll();

  // Wait for data to arrive at ENet socket B
  uint8_t buf[256] = {};
  auto start = std::chrono::steady_clock::now();
  ssize_t n = 0;
  while (n <= 0) {
    n = ::recv(kFdB, reinterpret_cast<char*>(buf), sizeof(buf), 0);
    if (n <= 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
      auto elapsed = std::chrono::steady_clock::now() - start;
      if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() >= 2000) break;
    }
  }
  REQUIRE(n == sizeof(kMsg));
  REQUIRE(std::memcmp(buf, kMsg, sizeof(kMsg)) == 0);

  bridge_a.Destroy();
  bridge_b.Destroy();
  agent_a.Stop();
  agent_b.Stop();
}

TEST_CASE("IceBridge destroy is safe", "[ice][bridge]") {
  IceAgent agent;
  IceAgent::Config cfg;
  cfg.stun_server = "";
  agent.Start(cfg);

  IceBridge bridge;
  bridge.Create(agent);
  bridge.Destroy();

  // Double destroy is safe
  bridge.Destroy();
  agent.Stop();
}
