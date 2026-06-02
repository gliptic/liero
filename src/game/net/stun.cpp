#include "stun.hpp"
#include "netutil.hpp"

#include <enet.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

using netutil::NowMs;

static constexpr const char* kStunServerIpV4 = "74.125.250.129";
static constexpr const char* kStunServerIpV6 = "2001:4860:4864:5:8000::1";
static constexpr uint16_t kStunPort = 19302;
static constexpr int kStunTimeoutMs = 2000;
static constexpr int kStunRetries = 2;

// --- stun namespace helpers ---

stun::Header stun::BuildRequest() {
  Header req = {};
  req.type = htons(kBindingRequest);
  req.length = 0;
  req.magic_cookie = htonl(kMagicCookie);

  std::random_device rd;
  for (int i = 0; i < 12; i++) req.transaction_id[i] = (uint8_t)(rd() & 0xFF);

  return req;
}

bool stun::IsResponse(const uint8_t* data, size_t len, const Header& req) {
  if (len < sizeof(Header)) return false;
  auto* resp = (const Header*)data;
  if (ntohs(resp->type) != kBindingResponse) return false;
  if (ntohl(resp->magic_cookie) != kMagicCookie) return false;
  return std::memcmp(resp->transaction_id, req.transaction_id, 12) == 0;
}

StunMappedAddress stun::ParseResponse(const uint8_t* data, size_t len, const stun::Header& req) {
  if (len < sizeof(stun::Header)) return {};

  auto* resp = (const stun::Header*)data;
  if (ntohs(resp->type) != stun::kBindingResponse) return {};
  if (ntohl(resp->magic_cookie) != stun::kMagicCookie) return {};
  if (std::memcmp(resp->transaction_id, req.transaction_id, 12) != 0) return {};

  uint16_t attr_total_len = ntohs(resp->length);
  if (sizeof(stun::Header) + attr_total_len > len) return {};

  const uint8_t* attrs = data + sizeof(stun::Header);
  size_t offset = 0;

  StunMappedAddress xor_result;
  StunMappedAddress plain_result;

  while (offset + 4 <= attr_total_len) {
    uint16_t attr_type = (uint16_t)(attrs[offset] << 8 | attrs[offset + 1]);
    uint16_t attr_len = (uint16_t)(attrs[offset + 2] << 8 | attrs[offset + 3]);
    offset += 4;

    if (offset + attr_len > attr_total_len) break;

    if (attr_type == stun::kAttrXorMappedAddress && attr_len >= 8) {
      uint8_t family = attrs[offset + 1];
      uint16_t xor_port = (uint16_t)(attrs[offset + 2] << 8 | attrs[offset + 3]);
      uint16_t mapped_port = xor_port ^ (uint16_t)(stun::kMagicCookie >> 16);

      if (family == 0x01 && attr_len >= 8) {
        uint32_t xor_addr;
        std::memcpy(&xor_addr, attrs + offset + 4, 4);
        uint32_t addr = ntohl(xor_addr) ^ stun::kMagicCookie;
        char buf[64];
        snprintf(buf, sizeof(buf), "%u.%u.%u.%u", (addr >> 24) & 0xFF, (addr >> 16) & 0xFF,
                 (addr >> 8) & 0xFF, addr & 0xFF);
        xor_result = {buf, mapped_port};
      } else if (family == 0x02 && attr_len >= 20) {
        uint8_t addr_bytes[16];
        std::memcpy(addr_bytes, attrs + offset + 4, 16);
        uint32_t cookie = htonl(stun::kMagicCookie);
        for (int i = 0; i < 4; i++) addr_bytes[i] ^= ((uint8_t*)&cookie)[i];
        for (int i = 0; i < 12; i++) addr_bytes[4 + i] ^= req.transaction_id[i];
        char buf[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, addr_bytes, buf, sizeof(buf));
        xor_result = {buf, mapped_port};
      }
    } else if (attr_type == stun::kAttrMappedAddress && attr_len >= 8) {
      uint8_t family = attrs[offset + 1];
      uint16_t mapped_port = (uint16_t)(attrs[offset + 2] << 8 | attrs[offset + 3]);

      if (family == 0x01) {
        uint32_t addr;
        std::memcpy(&addr, attrs + offset + 4, 4);
        addr = ntohl(addr);
        char buf[64];
        snprintf(buf, sizeof(buf), "%u.%u.%u.%u", (addr >> 24) & 0xFF, (addr >> 16) & 0xFF,
                 (addr >> 8) & 0xFF, addr & 0xFF);
        plain_result = {buf, mapped_port};
      }
    }

    offset += (attr_len + 3) & ~3u;
  }

  if (!xor_result.ip.empty()) return xor_result;
  return plain_result;
}

// --- StunQuery (background thread, own socket) ---

void StunQuery::Start() {
  if (started_.exchange(true)) return;
  thread_ = std::thread(&StunQuery::Run, this);
}

void StunQuery::Start(uint16_t local_port) {
  if (started_.exchange(true)) return;
  localPort_ = local_port;
  thread_ = std::thread(&StunQuery::Run, this);
}

StunQuery::~StunQuery() {
  if (thread_.joinable()) thread_.join();
}

StunResult StunQuery::Result() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return result_;
}

StunMappedAddress StunQuery::QueryServer(const char* server_addr, uint16_t port,
                                         uint16_t local_port) {
  enet_initialize();

  ENetSocket sock = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
  if (sock == ENET_SOCKET_NULL) return {};

  if (local_port != 0) {
    enet_socket_set_option(sock, ENET_SOCKOPT_REUSEADDR, 1);
    ENetAddress local_addr = {};
    local_addr.port = local_port;
    memset(&local_addr.host, 0, sizeof(local_addr.host));
    enet_socket_bind(sock, &local_addr);
  }

  ENetAddress addr = {};
  addr.port = port;
  if (enet_address_set_host(&addr, server_addr) != 0) {
    enet_socket_destroy(sock);
    return {};
  }

  stun::Header req = stun::BuildRequest();

  ENetBuffer send_buf;
  send_buf.data = &req;
  send_buf.dataLength = sizeof(req);

  uint8_t recv_data[512];
  ENetBuffer recv_buf;
  recv_buf.data = recv_data;
  recv_buf.dataLength = sizeof(recv_data);

  StunMappedAddress result;
  for (int attempt = 0; attempt < kStunRetries && result.ip.empty(); ++attempt) {
    int sent = enet_socket_send(sock, &addr, &send_buf, 1);
    if (sent < 0) break;

    enet_uint32 wait_condition = ENET_SOCKET_WAIT_RECEIVE;
    if (enet_socket_wait(sock, &wait_condition, kStunTimeoutMs) != 0) continue;
    if (!(wait_condition & ENET_SOCKET_WAIT_RECEIVE)) continue;

    ENetAddress from_addr = {};
    int recv_len = enet_socket_receive(sock, &from_addr, &recv_buf, 1);
    if (recv_len < (int)sizeof(stun::Header)) continue;

    result = stun::ParseResponse(recv_data, (size_t)recv_len, req);
  }

  enet_socket_destroy(sock);
  return result;
}

void StunQuery::Run() {
  StunResult res;
  auto v4 = QueryServer(kStunServerIpV4, kStunPort, localPort_);
  res.ipv4 = v4.ip;
  res.ipv4_port = v4.port;
  auto v6 = QueryServer(kStunServerIpV6, kStunPort, localPort_);
  res.ipv6 = v6.ip;
  res.ipv6_port = v6.port;

  {
    std::lock_guard<std::mutex> lock(mutex_);
    result_ = res;
  }
  done_.store(true);
}
