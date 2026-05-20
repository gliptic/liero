#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <mutex>

struct StunResult {
  std::string ipv4; // External IPv4, or empty if unavailable
  std::string ipv6; // External IPv6, or empty if unavailable
  uint16_t ipv4Port = 0; // External port from IPv4 STUN response
  uint16_t ipv6Port = 0; // External port from IPv6 STUN response
};

// Minimal STUN client (RFC 5389) for discovering external IP addresses.
// Queries a public STUN server over both IPv4 and IPv6 to discover
// the external address for each protocol.
class StunQuery {
public:
  void start();

  // Returns the results collected so far.
  StunResult result() const;

  // True if the query has completed (success or failure).
  bool done() const { return done_.load(); }

  ~StunQuery();

  struct StunAddress { std::string ip; uint16_t port = 0; };

private:
  // Query a specific STUN server address (IPv4 or IPv6 literal)
  static StunAddress queryServer(const char* serverAddr, uint16_t port);

  void run();

  std::thread thread_;
  std::atomic<bool> done_{false};
  mutable std::mutex mutex_;
  StunResult result_;
};
