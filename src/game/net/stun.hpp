#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

struct StunResult {
  std::string ipv4;
  uint16_t ipv4_port = 0;
  std::string ipv6;
  uint16_t ipv6_port = 0;
};

struct StunMappedAddress {
  std::string ip;
  uint16_t port = 0;
};

// STUN protocol constants (RFC 5389)
namespace stun {
constexpr uint16_t kBindingRequest = 0x0001;
constexpr uint16_t kBindingResponse = 0x0101;
constexpr uint32_t kMagicCookie = 0x2112A442;
constexpr uint16_t kAttrXorMappedAddress = 0x0020;
constexpr uint16_t kAttrMappedAddress = 0x0001;

struct Header {
  uint16_t type;
  uint16_t length;
  uint32_t magic_cookie;
  uint8_t transaction_id[12];
};
static_assert(sizeof(Header) == 20);

// Parse a STUN Binding Response, extracting the mapped address.
StunMappedAddress ParseResponse(const uint8_t* data, size_t len, const Header& req);

// Build a STUN Binding Request. Fills in txnId with random bytes.
Header BuildRequest();

// Check if a packet is a STUN Binding Response matching our transaction ID.
bool IsResponse(const uint8_t* data, size_t len, const Header& req);
}  // namespace stun

// Async STUN query using a background thread and its own socket.
// Use this for initial discovery when no ENet host exists yet.
class StunQuery {
 public:
  void Start();
  void Start(uint16_t local_port);

  StunResult Result() const;
  bool Done() const { return done_.load(); }

  ~StunQuery();

 private:
  static StunMappedAddress QueryServer(const char* server_addr, uint16_t port,
                                       uint16_t local_port = 0);
  void Run();

  std::thread thread_;
  std::atomic<bool> done_{false};
  std::atomic<bool> started_{false};
  mutable std::mutex mutex_;
  StunResult result_;
  uint16_t localPort_{0};
};
