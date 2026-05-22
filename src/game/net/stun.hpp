#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <cstdint>

struct StunResult {
  std::string ipv4;
  uint16_t ipv4Port = 0;
  std::string ipv6;
  uint16_t ipv6Port = 0;
};

struct StunMappedAddress {
  std::string ip;
  uint16_t port = 0;
};

// STUN protocol constants (RFC 5389)
namespace stun {
  constexpr uint16_t BINDING_REQUEST = 0x0001;
  constexpr uint16_t BINDING_RESPONSE = 0x0101;
  constexpr uint32_t MAGIC_COOKIE = 0x2112A442;
  constexpr uint16_t ATTR_XOR_MAPPED_ADDRESS = 0x0020;
  constexpr uint16_t ATTR_MAPPED_ADDRESS = 0x0001;

  struct Header {
    uint16_t type;
    uint16_t length;
    uint32_t magicCookie;
    uint8_t transactionId[12];
  };
  static_assert(sizeof(Header) == 20);

  // Parse a STUN Binding Response, extracting the mapped address.
  StunMappedAddress parseResponse(const uint8_t* data, size_t len, const Header& req);

  // Build a STUN Binding Request. Fills in txnId with random bytes.
  Header buildRequest();

  // Check if a packet is a STUN Binding Response matching our transaction ID.
  bool isResponse(const uint8_t* data, size_t len, const Header& req);
}

// Async STUN query using a background thread and its own socket.
// Use this for initial discovery when no ENet host exists yet.
class StunQuery {
public:
  void start();
  void start(uint16_t localPort);

  StunResult result() const;
  bool done() const { return done_.load(); }

  ~StunQuery();

private:
  static StunMappedAddress queryServer(const char* serverAddr, uint16_t port,
                                        uint16_t localPort = 0);
  void run();

  std::thread thread_;
  std::atomic<bool> done_{false};
  std::atomic<bool> started_{false};
  mutable std::mutex mutex_;
  StunResult result_;
  uint16_t localPort_{0};
};

