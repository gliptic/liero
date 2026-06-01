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
static std::vector<uint8_t> buildIPv4Response(const stun::Header& req, const char* ip,
                                              uint16_t port) {
  // Parse IP into network-order bytes
  uint32_t ipNet;
  inet_pton(AF_INET, ip, &ipNet);
  uint32_t ipHost = ntohl(ipNet);

  // XOR the address and port
  uint32_t xorAddr = htonl(ipHost ^ stun::MAGIC_COOKIE);
  uint16_t xorPort = port ^ (uint16_t)(stun::MAGIC_COOKIE >> 16);

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
  attr[6] = (uint8_t)(xorPort >> 8);
  attr[7] = (uint8_t)(xorPort & 0xFF);
  std::memcpy(attr + 8, &xorAddr, 4);

  pkt.insert(pkt.end(), attr, attr + 12);

  // Fill header
  auto* hdr = (stun::Header*)pkt.data();
  hdr->type = htons(stun::BINDING_RESPONSE);
  hdr->length = htons(12);  // attribute total length
  hdr->magicCookie = htonl(stun::MAGIC_COOKIE);
  std::memcpy(hdr->transactionId, req.transactionId, 12);

  return pkt;
}

// Helper to build a STUN Binding Response with XOR-MAPPED-ADDRESS (IPv6)
static std::vector<uint8_t> buildIPv6Response(const stun::Header& req, const char* ip,
                                              uint16_t port) {
  uint8_t ipBytes[16];
  inet_pton(AF_INET6, ip, ipBytes);

  // XOR with magic cookie (4 bytes) + transaction ID (12 bytes)
  uint32_t cookie = htonl(stun::MAGIC_COOKIE);
  uint8_t xorAddr[16];
  std::memcpy(xorAddr, ipBytes, 16);
  for (int i = 0; i < 4; i++) xorAddr[i] ^= ((uint8_t*)&cookie)[i];
  for (int i = 0; i < 12; i++) xorAddr[4 + i] ^= req.transactionId[i];

  uint16_t xorPort = port ^ (uint16_t)(stun::MAGIC_COOKIE >> 16);

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
  attr[6] = (uint8_t)(xorPort >> 8);
  attr[7] = (uint8_t)(xorPort & 0xFF);
  std::memcpy(attr + 8, xorAddr, 16);

  pkt.insert(pkt.end(), attr, attr + 24);

  // Fill header
  auto* hdr = (stun::Header*)pkt.data();
  hdr->type = htons(stun::BINDING_RESPONSE);
  hdr->length = htons(24);
  hdr->magicCookie = htonl(stun::MAGIC_COOKIE);
  std::memcpy(hdr->transactionId, req.transactionId, 12);

  return pkt;
}

// Helper to build a STUN Binding Response with MAPPED-ADDRESS (non-XOR, fallback)
static std::vector<uint8_t> buildMappedAddressResponse(const stun::Header& req, const char* ip,
                                                       uint16_t port) {
  uint32_t ipNet;
  inet_pton(AF_INET, ip, &ipNet);

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
  std::memcpy(attr + 8, &ipNet, 4);  // network byte order

  pkt.insert(pkt.end(), attr, attr + 12);

  auto* hdr = (stun::Header*)pkt.data();
  hdr->type = htons(stun::BINDING_RESPONSE);
  hdr->length = htons(12);
  hdr->magicCookie = htonl(stun::MAGIC_COOKIE);
  std::memcpy(hdr->transactionId, req.transactionId, 12);

  return pkt;
}

static stun::Header makeRequest() {
  stun::Header req = {};
  req.type = htons(stun::BINDING_REQUEST);
  req.length = 0;
  req.magicCookie = htonl(stun::MAGIC_COOKIE);
  // Use a known transaction ID for reproducibility
  for (int i = 0; i < 12; i++) req.transactionId[i] = (uint8_t)(0x10 + i);
  return req;
}

TEST_CASE("parseResponse extracts IPv4 XOR-MAPPED-ADDRESS", "[stun]") {
  auto req = makeRequest();
  auto pkt = buildIPv4Response(req, "203.0.113.42", 54321);

  auto result = stun::parseResponse(pkt.data(), pkt.size(), req);
  REQUIRE(result.ip == "203.0.113.42");
  REQUIRE(result.port == 54321);
}

TEST_CASE("parseResponse extracts IPv6 XOR-MAPPED-ADDRESS", "[stun]") {
  auto req = makeRequest();
  auto pkt = buildIPv6Response(req, "2001:db8::1", 12345);

  auto result = stun::parseResponse(pkt.data(), pkt.size(), req);
  REQUIRE(result.ip == "2001:db8::1");
  REQUIRE(result.port == 12345);
}

TEST_CASE("parseResponse falls back to MAPPED-ADDRESS", "[stun]") {
  auto req = makeRequest();
  auto pkt = buildMappedAddressResponse(req, "198.51.100.7", 9999);

  auto result = stun::parseResponse(pkt.data(), pkt.size(), req);
  REQUIRE(result.ip == "198.51.100.7");
  REQUIRE(result.port == 9999);
}

TEST_CASE("parseResponse prefers XOR-MAPPED-ADDRESS over MAPPED-ADDRESS", "[stun]") {
  auto req = makeRequest();

  // Build a response with MAPPED-ADDRESS first, then XOR-MAPPED-ADDRESS
  std::vector<uint8_t> pkt;
  pkt.resize(sizeof(stun::Header));

  // MAPPED-ADDRESS with wrong IP (should be ignored if XOR-MAPPED follows)
  uint8_t mappedAttr[12] = {};
  mappedAttr[0] = 0x00;
  mappedAttr[1] = 0x01;  // MAPPED-ADDRESS
  mappedAttr[2] = 0x00;
  mappedAttr[3] = 0x08;
  mappedAttr[4] = 0x00;
  mappedAttr[5] = 0x01;  // IPv4
  mappedAttr[6] = 0x00;
  mappedAttr[7] = 0x50;  // port 80
  uint32_t wrongIP;
  inet_pton(AF_INET, "10.0.0.1", &wrongIP);
  std::memcpy(mappedAttr + 8, &wrongIP, 4);
  pkt.insert(pkt.end(), mappedAttr, mappedAttr + 12);

  // XOR-MAPPED-ADDRESS with correct IP
  uint32_t ipNet;
  inet_pton(AF_INET, "203.0.113.42", &ipNet);
  uint32_t ipHost = ntohl(ipNet);
  uint32_t xorAddr = htonl(ipHost ^ stun::MAGIC_COOKIE);
  uint16_t xorPort = 54321 ^ (uint16_t)(stun::MAGIC_COOKIE >> 16);

  uint8_t xorAttr[12] = {};
  xorAttr[0] = 0x00;
  xorAttr[1] = 0x20;  // XOR-MAPPED-ADDRESS
  xorAttr[2] = 0x00;
  xorAttr[3] = 0x08;
  xorAttr[4] = 0x00;
  xorAttr[5] = 0x01;  // IPv4
  xorAttr[6] = (uint8_t)(xorPort >> 8);
  xorAttr[7] = (uint8_t)(xorPort & 0xFF);
  std::memcpy(xorAttr + 8, &xorAddr, 4);
  pkt.insert(pkt.end(), xorAttr, xorAttr + 12);

  auto* hdr = (stun::Header*)pkt.data();
  hdr->type = htons(stun::BINDING_RESPONSE);
  hdr->length = htons(24);  // 12 + 12
  hdr->magicCookie = htonl(stun::MAGIC_COOKIE);
  std::memcpy(hdr->transactionId, req.transactionId, 12);

  auto result = stun::parseResponse(pkt.data(), pkt.size(), req);

  // RFC 5389: XOR-MAPPED-ADDRESS is preferred over MAPPED-ADDRESS.
  // Even though MAPPED-ADDRESS comes first, we should get the XOR result.
  REQUIRE(result.ip == "203.0.113.42");
  REQUIRE(result.port == 54321);
}

TEST_CASE("parseResponse rejects wrong transaction ID", "[stun]") {
  auto req = makeRequest();
  auto pkt = buildIPv4Response(req, "203.0.113.42", 54321);

  // Corrupt the transaction ID in the request we pass to parseResponse
  stun::Header wrongReq = req;
  wrongReq.transactionId[0] = 0xFF;

  auto result = stun::parseResponse(pkt.data(), pkt.size(), wrongReq);
  REQUIRE(result.ip.empty());
  REQUIRE(result.port == 0);
}

TEST_CASE("parseResponse rejects non-binding-response", "[stun]") {
  auto req = makeRequest();
  auto pkt = buildIPv4Response(req, "203.0.113.42", 54321);

  // Change type to something else
  auto* hdr = (stun::Header*)pkt.data();
  hdr->type = htons(0x0111);  // Binding Error Response

  auto result = stun::parseResponse(pkt.data(), pkt.size(), req);
  REQUIRE(result.ip.empty());
}

TEST_CASE("parseResponse rejects truncated packet", "[stun]") {
  auto req = makeRequest();

  // Too short — less than header size
  uint8_t tiny[10] = {};
  auto result = stun::parseResponse(tiny, sizeof(tiny), req);
  REQUIRE(result.ip.empty());
}

TEST_CASE("parseResponse handles empty attributes", "[stun]") {
  auto req = makeRequest();

  // Valid header but no attributes
  std::vector<uint8_t> pkt(sizeof(stun::Header));
  auto* hdr = (stun::Header*)pkt.data();
  hdr->type = htons(stun::BINDING_RESPONSE);
  hdr->length = htons(0);
  hdr->magicCookie = htonl(stun::MAGIC_COOKIE);
  std::memcpy(hdr->transactionId, req.transactionId, 12);

  auto result = stun::parseResponse(pkt.data(), pkt.size(), req);
  REQUIRE(result.ip.empty());
  REQUIRE(result.port == 0);
}

TEST_CASE("parseResponse handles padded attributes", "[stun]") {
  auto req = makeRequest();

  // Build a response with a 5-byte unknown attribute (padded to 8) followed by XOR-MAPPED-ADDRESS
  std::vector<uint8_t> pkt;
  pkt.resize(sizeof(stun::Header));

  // Unknown attribute: type=0x8000, length=5 (padded to 8 bytes)
  uint8_t unknownAttr[4 + 8] = {};  // 4 header + 8 padded data
  unknownAttr[0] = 0x80;
  unknownAttr[1] = 0x00;  // unknown type
  unknownAttr[2] = 0x00;
  unknownAttr[3] = 0x05;  // length = 5
  // 5 bytes data + 3 bytes padding = 8 bytes
  pkt.insert(pkt.end(), unknownAttr, unknownAttr + 12);

  // XOR-MAPPED-ADDRESS
  uint32_t ipNet;
  inet_pton(AF_INET, "192.0.2.1", &ipNet);
  uint32_t ipHost = ntohl(ipNet);
  uint32_t xorAddr = htonl(ipHost ^ stun::MAGIC_COOKIE);
  uint16_t xorPort = 8080 ^ (uint16_t)(stun::MAGIC_COOKIE >> 16);

  uint8_t xorAttr[12] = {};
  xorAttr[0] = 0x00;
  xorAttr[1] = 0x20;
  xorAttr[2] = 0x00;
  xorAttr[3] = 0x08;
  xorAttr[4] = 0x00;
  xorAttr[5] = 0x01;
  xorAttr[6] = (uint8_t)(xorPort >> 8);
  xorAttr[7] = (uint8_t)(xorPort & 0xFF);
  std::memcpy(xorAttr + 8, &xorAddr, 4);
  pkt.insert(pkt.end(), xorAttr, xorAttr + 12);

  auto* hdr = (stun::Header*)pkt.data();
  hdr->type = htons(stun::BINDING_RESPONSE);
  hdr->length = htons(24);  // 12 (unknown padded) + 12 (xor-mapped)
  hdr->magicCookie = htonl(stun::MAGIC_COOKIE);
  std::memcpy(hdr->transactionId, req.transactionId, 12);

  auto result = stun::parseResponse(pkt.data(), pkt.size(), req);
  REQUIRE(result.ip == "192.0.2.1");
  REQUIRE(result.port == 8080);
}

TEST_CASE("parseResponse handles port 0 and port 65535", "[stun]") {
  auto req = makeRequest();

  SECTION("port 0") {
    auto pkt = buildIPv4Response(req, "10.0.0.1", 0);
    auto result = stun::parseResponse(pkt.data(), pkt.size(), req);
    REQUIRE(result.ip == "10.0.0.1");
    REQUIRE(result.port == 0);
  }

  SECTION("port 65535") {
    auto pkt = buildIPv4Response(req, "10.0.0.1", 65535);
    auto result = stun::parseResponse(pkt.data(), pkt.size(), req);
    REQUIRE(result.ip == "10.0.0.1");
    REQUIRE(result.port == 65535);
  }
}

TEST_CASE("StunQuery completes with done flag", "[stun][integration]") {
  // This test requires network access to Google's STUN server.
  // It will gracefully fail (empty results) if network is unavailable.
  StunQuery query;
  query.start();

  // Wait up to 6 seconds for completion
  for (int i = 0; i < 60 && !query.done(); i++)
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

  REQUIRE(query.done());

  auto result = query.result();
  // We can't guarantee STUN succeeds in all test environments,
  // but the query must complete without crashing.
  // If network is available, we should get at least an IPv4 result.
  if (!result.ipv4.empty()) {
    REQUIRE(result.ipv4Port != 0);
    // Sanity check: IP should have dots
    REQUIRE(result.ipv4.find('.') != std::string::npos);
  }
}
