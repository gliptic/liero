#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include "net/stun.hpp"

// Helper to build a STUN Binding Response with XOR-MAPPED-ADDRESS (IPv4)
static std::vector<uint8_t> BuildIPv4Response(const stun::Header& req, const char* ip,
                                              uint16_t port) {
  // Parse IP into network-order bytes
  uint32_t ip_net;
  inet_pton(AF_INET, ip, &ip_net);
  uint32_t ip_host = ntohl(ip_net);

  // XOR the address and port
  uint32_t xor_addr = htonl(ip_host ^ stun::kMagicCookie);
  uint16_t xor_port = port ^ (uint16_t)(stun::kMagicCookie >> 16);

  // Build response
  std::vector<uint8_t> pkt;
  pkt.resize(sizeof(stun::Header));

  // XOR-MAPPED-ADDRESS attribute: type(2) + len(2) + reserved(1) + family(1) + port(2) + addr(4) =
  // 12 bytes
  uint8_t attr[12] = {};
  attr[0] = 0x00;
  attr[1] = 0x20;  // type = XOR-MAPPED-ADDRESS
  attr[2] = 0x00;
  attr[3] = 0x08;  // length = 8
  attr[4] = 0x00;  // reserved
  attr[5] = 0x01;  // family = IPv4
  attr[6] = (uint8_t)(xor_port >> 8);
  attr[7] = (uint8_t)(xor_port & 0xFF);
  std::memcpy(attr + 8, &xor_addr, 4);

  pkt.insert(pkt.end(), attr, attr + 12);

  // Fill header
  auto* hdr = (stun::Header*)pkt.data();
  hdr->type = htons(stun::kBindingResponse);
  hdr->length = htons(12);  // attribute total length
  hdr->magic_cookie = htonl(stun::kMagicCookie);
  std::memcpy(hdr->transaction_id, req.transaction_id, 12);

  return pkt;
}

// Helper to build a STUN Binding Response with XOR-MAPPED-ADDRESS (IPv6)
static std::vector<uint8_t> BuildIPv6Response(const stun::Header& req, const char* ip,
                                              uint16_t port) {
  uint8_t ip_bytes[16];
  inet_pton(AF_INET6, ip, ip_bytes);

  // XOR with magic cookie (4 bytes) + transaction ID (12 bytes)
  uint32_t cookie = htonl(stun::kMagicCookie);
  uint8_t xor_addr[16];
  std::memcpy(xor_addr, ip_bytes, 16);
  for (int i = 0; i < 4; i++) xor_addr[i] ^= ((uint8_t*)&cookie)[i];
  for (int i = 0; i < 12; i++) xor_addr[4 + i] ^= req.transaction_id[i];

  uint16_t xor_port = port ^ (uint16_t)(stun::kMagicCookie >> 16);

  // Build response
  std::vector<uint8_t> pkt;
  pkt.resize(sizeof(stun::Header));

  // XOR-MAPPED-ADDRESS for IPv6: type(2) + len(2) + reserved(1) + family(1) + port(2) + addr(16) =
  // 24 bytes
  uint8_t attr[24] = {};
  attr[0] = 0x00;
  attr[1] = 0x20;  // type = XOR-MAPPED-ADDRESS
  attr[2] = 0x00;
  attr[3] = 20;    // length = 20
  attr[4] = 0x00;  // reserved
  attr[5] = 0x02;  // family = IPv6
  attr[6] = (uint8_t)(xor_port >> 8);
  attr[7] = (uint8_t)(xor_port & 0xFF);
  std::memcpy(attr + 8, xor_addr, 16);

  pkt.insert(pkt.end(), attr, attr + 24);

  // Fill header
  auto* hdr = (stun::Header*)pkt.data();
  hdr->type = htons(stun::kBindingResponse);
  hdr->length = htons(24);
  hdr->magic_cookie = htonl(stun::kMagicCookie);
  std::memcpy(hdr->transaction_id, req.transaction_id, 12);

  return pkt;
}

// Helper to build a STUN Binding Response with MAPPED-ADDRESS (non-XOR, fallback)
static std::vector<uint8_t> BuildMappedAddressResponse(const stun::Header& req, const char* ip,
                                                       uint16_t port) {
  uint32_t ip_net;
  inet_pton(AF_INET, ip, &ip_net);

  std::vector<uint8_t> pkt;
  pkt.resize(sizeof(stun::Header));

  // MAPPED-ADDRESS: type(2) + len(2) + reserved(1) + family(1) + port(2) + addr(4) = 12 bytes
  uint8_t attr[12] = {};
  attr[0] = 0x00;
  attr[1] = 0x01;  // type = MAPPED-ADDRESS
  attr[2] = 0x00;
  attr[3] = 0x08;  // length = 8
  attr[4] = 0x00;  // reserved
  attr[5] = 0x01;  // family = IPv4
  attr[6] = (uint8_t)(port >> 8);
  attr[7] = (uint8_t)(port & 0xFF);
  std::memcpy(attr + 8, &ip_net, 4);  // network byte order

  pkt.insert(pkt.end(), attr, attr + 12);

  auto* hdr = (stun::Header*)pkt.data();
  hdr->type = htons(stun::kBindingResponse);
  hdr->length = htons(12);
  hdr->magic_cookie = htonl(stun::kMagicCookie);
  std::memcpy(hdr->transaction_id, req.transaction_id, 12);

  return pkt;
}

static stun::Header MakeRequest() {
  stun::Header req = {};
  req.type = htons(stun::kBindingRequest);
  req.length = 0;
  req.magic_cookie = htonl(stun::kMagicCookie);
  // Use a known transaction ID for reproducibility
  for (int i = 0; i < 12; i++) req.transaction_id[i] = (uint8_t)(0x10 + i);
  return req;
}

TEST_CASE("parseResponse extracts IPv4 XOR-MAPPED-ADDRESS", "[stun]") {
  auto req = MakeRequest();
  auto pkt = BuildIPv4Response(req, "203.0.113.42", 54321);

  auto result = stun::ParseResponse(pkt.data(), pkt.size(), req);
  REQUIRE(result.ip == "203.0.113.42");
  REQUIRE(result.port == 54321);
}

TEST_CASE("parseResponse extracts IPv6 XOR-MAPPED-ADDRESS", "[stun]") {
  auto req = MakeRequest();
  auto pkt = BuildIPv6Response(req, "2001:db8::1", 12345);

  auto result = stun::ParseResponse(pkt.data(), pkt.size(), req);
  REQUIRE(result.ip == "2001:db8::1");
  REQUIRE(result.port == 12345);
}

TEST_CASE("parseResponse falls back to MAPPED-ADDRESS", "[stun]") {
  auto req = MakeRequest();
  auto pkt = BuildMappedAddressResponse(req, "198.51.100.7", 9999);

  auto result = stun::ParseResponse(pkt.data(), pkt.size(), req);
  REQUIRE(result.ip == "198.51.100.7");
  REQUIRE(result.port == 9999);
}

TEST_CASE("parseResponse prefers XOR-MAPPED-ADDRESS over MAPPED-ADDRESS", "[stun]") {
  auto req = MakeRequest();

  // Build a response with MAPPED-ADDRESS first, then XOR-MAPPED-ADDRESS
  std::vector<uint8_t> pkt;
  pkt.resize(sizeof(stun::Header));

  // MAPPED-ADDRESS with wrong IP (should be ignored if XOR-MAPPED follows)
  uint8_t mapped_attr[12] = {};
  mapped_attr[0] = 0x00;
  mapped_attr[1] = 0x01;  // MAPPED-ADDRESS
  mapped_attr[2] = 0x00;
  mapped_attr[3] = 0x08;
  mapped_attr[4] = 0x00;
  mapped_attr[5] = 0x01;  // IPv4
  mapped_attr[6] = 0x00;
  mapped_attr[7] = 0x50;  // port 80
  uint32_t wrong_ip;
  inet_pton(AF_INET, "10.0.0.1", &wrong_ip);
  std::memcpy(mapped_attr + 8, &wrong_ip, 4);
  pkt.insert(pkt.end(), mapped_attr, mapped_attr + 12);

  // XOR-MAPPED-ADDRESS with correct IP
  uint32_t ip_net;
  inet_pton(AF_INET, "203.0.113.42", &ip_net);
  uint32_t ip_host = ntohl(ip_net);
  uint32_t xor_addr = htonl(ip_host ^ stun::kMagicCookie);
  uint16_t xor_port = 54321 ^ (uint16_t)(stun::kMagicCookie >> 16);

  uint8_t xor_attr[12] = {};
  xor_attr[0] = 0x00;
  xor_attr[1] = 0x20;  // XOR-MAPPED-ADDRESS
  xor_attr[2] = 0x00;
  xor_attr[3] = 0x08;
  xor_attr[4] = 0x00;
  xor_attr[5] = 0x01;  // IPv4
  xor_attr[6] = (uint8_t)(xor_port >> 8);
  xor_attr[7] = (uint8_t)(xor_port & 0xFF);
  std::memcpy(xor_attr + 8, &xor_addr, 4);
  pkt.insert(pkt.end(), xor_attr, xor_attr + 12);

  auto* hdr = (stun::Header*)pkt.data();
  hdr->type = htons(stun::kBindingResponse);
  hdr->length = htons(24);  // 12 + 12
  hdr->magic_cookie = htonl(stun::kMagicCookie);
  std::memcpy(hdr->transaction_id, req.transaction_id, 12);

  auto result = stun::ParseResponse(pkt.data(), pkt.size(), req);

  // RFC 5389: XOR-MAPPED-ADDRESS is preferred over MAPPED-ADDRESS.
  // Even though MAPPED-ADDRESS comes first, we should get the XOR result.
  REQUIRE(result.ip == "203.0.113.42");
  REQUIRE(result.port == 54321);
}

TEST_CASE("parseResponse rejects wrong transaction ID", "[stun]") {
  auto req = MakeRequest();
  auto pkt = BuildIPv4Response(req, "203.0.113.42", 54321);

  // Corrupt the transaction ID in the request we pass to parseResponse
  stun::Header wrong_req = req;
  wrong_req.transaction_id[0] = 0xFF;

  auto result = stun::ParseResponse(pkt.data(), pkt.size(), wrong_req);
  REQUIRE(result.ip.empty());
  REQUIRE(result.port == 0);
}

TEST_CASE("parseResponse rejects non-binding-response", "[stun]") {
  auto req = MakeRequest();
  auto pkt = BuildIPv4Response(req, "203.0.113.42", 54321);

  // Change type to something else
  auto* hdr = (stun::Header*)pkt.data();
  hdr->type = htons(0x0111);  // Binding Error Response

  auto result = stun::ParseResponse(pkt.data(), pkt.size(), req);
  REQUIRE(result.ip.empty());
}

TEST_CASE("parseResponse rejects truncated packet", "[stun]") {
  auto req = MakeRequest();

  // Too short — less than header size
  uint8_t tiny[10] = {};
  auto result = stun::ParseResponse(tiny, sizeof(tiny), req);
  REQUIRE(result.ip.empty());
}

TEST_CASE("parseResponse handles empty attributes", "[stun]") {
  auto req = MakeRequest();

  // Valid header but no attributes
  std::vector<uint8_t> pkt(sizeof(stun::Header));
  auto* hdr = (stun::Header*)pkt.data();
  hdr->type = htons(stun::kBindingResponse);
  hdr->length = htons(0);
  hdr->magic_cookie = htonl(stun::kMagicCookie);
  std::memcpy(hdr->transaction_id, req.transaction_id, 12);

  auto result = stun::ParseResponse(pkt.data(), pkt.size(), req);
  REQUIRE(result.ip.empty());
  REQUIRE(result.port == 0);
}

TEST_CASE("parseResponse handles padded attributes", "[stun]") {
  auto req = MakeRequest();

  // Build a response with a 5-byte unknown attribute (padded to 8) followed by XOR-MAPPED-ADDRESS
  std::vector<uint8_t> pkt;
  pkt.resize(sizeof(stun::Header));

  // Unknown attribute: type=0x8000, length=5 (padded to 8 bytes)
  uint8_t unknown_attr[4 + 8] = {};  // 4 header + 8 padded data
  unknown_attr[0] = 0x80;
  unknown_attr[1] = 0x00;  // unknown type
  unknown_attr[2] = 0x00;
  unknown_attr[3] = 0x05;  // length = 5
  // 5 bytes data + 3 bytes padding = 8 bytes
  pkt.insert(pkt.end(), unknown_attr, unknown_attr + 12);

  // XOR-MAPPED-ADDRESS
  uint32_t ip_net;
  inet_pton(AF_INET, "192.0.2.1", &ip_net);
  uint32_t ip_host = ntohl(ip_net);
  uint32_t xor_addr = htonl(ip_host ^ stun::kMagicCookie);
  uint16_t xor_port = 8080 ^ (uint16_t)(stun::kMagicCookie >> 16);

  uint8_t xor_attr[12] = {};
  xor_attr[0] = 0x00;
  xor_attr[1] = 0x20;
  xor_attr[2] = 0x00;
  xor_attr[3] = 0x08;
  xor_attr[4] = 0x00;
  xor_attr[5] = 0x01;
  xor_attr[6] = (uint8_t)(xor_port >> 8);
  xor_attr[7] = (uint8_t)(xor_port & 0xFF);
  std::memcpy(xor_attr + 8, &xor_addr, 4);
  pkt.insert(pkt.end(), xor_attr, xor_attr + 12);

  auto* hdr = (stun::Header*)pkt.data();
  hdr->type = htons(stun::kBindingResponse);
  hdr->length = htons(24);  // 12 (unknown padded) + 12 (xor-mapped)
  hdr->magic_cookie = htonl(stun::kMagicCookie);
  std::memcpy(hdr->transaction_id, req.transaction_id, 12);

  auto result = stun::ParseResponse(pkt.data(), pkt.size(), req);
  REQUIRE(result.ip == "192.0.2.1");
  REQUIRE(result.port == 8080);
}

TEST_CASE("parseResponse handles port 0 and port 65535", "[stun]") {
  auto req = MakeRequest();

  SECTION("port 0") {
    auto pkt = BuildIPv4Response(req, "10.0.0.1", 0);
    auto result = stun::ParseResponse(pkt.data(), pkt.size(), req);
    REQUIRE(result.ip == "10.0.0.1");
    REQUIRE(result.port == 0);
  }

  SECTION("port 65535") {
    auto pkt = BuildIPv4Response(req, "10.0.0.1", 65535);
    auto result = stun::ParseResponse(pkt.data(), pkt.size(), req);
    REQUIRE(result.ip == "10.0.0.1");
    REQUIRE(result.port == 65535);
  }
}

TEST_CASE("StunQuery completes with done flag", "[stun][integration]") {
  // This test requires network access to Google's STUN server.
  // It will gracefully fail (empty results) if network is unavailable.
  StunQuery query;
  query.Start();

  // Wait up to 6 seconds for completion
  for (int i = 0; i < 60 && !query.Done(); i++)
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

  REQUIRE(query.Done());

  auto result = query.Result();
  // We can't guarantee STUN succeeds in all test environments,
  // but the query must complete without crashing.
  // If network is available, we should get at least an IPv4 result.
  if (!result.ipv4.empty()) {
    REQUIRE(result.ipv4_port != 0);
    // Sanity check: IP should have dots
    REQUIRE(result.ipv4.find('.') != std::string::npos);
  }
}
