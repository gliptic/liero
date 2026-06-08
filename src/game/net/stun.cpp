#include "stun.hpp"
#include "netutil.hpp"

#include <enet.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <utility>

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

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
  for (unsigned char& i : req.transaction_id) {
    i = static_cast<uint8_t>(rd() & 0xFF);
  }

  return req;
}

bool stun::IsResponse(const uint8_t* data, size_t len, const Header& req) {
  if (len < sizeof(Header)) {
    return false;
  }
  const auto* resp = reinterpret_cast<const Header*>(data);
  if (ntohs(resp->type) != kBindingResponse) {
    return false;
  }
  if (ntohl(resp->magic_cookie) != kMagicCookie) {
    return false;
  }
  return std::memcmp(resp->transaction_id, req.transaction_id, 12) == 0;
}

StunMappedAddress stun::ParseResponse(const uint8_t* data, size_t len, const stun::Header& req) {
  if (len < sizeof(stun::Header)) {
    return {};
  }

  const auto* resp = reinterpret_cast<const stun::Header*>(data);
  if (ntohs(resp->type) != stun::kBindingResponse) {
    return {};
  }
  if (ntohl(resp->magic_cookie) != stun::kMagicCookie) {
    return {};
  }
  if (std::memcmp(resp->transaction_id, req.transaction_id, 12) != 0) {
    return {};
  }

  uint16_t const kAttrTotalLen = ntohs(resp->length);
  if (sizeof(stun::Header) + kAttrTotalLen > len) {
    return {};
  }

  const uint8_t* attrs = data + sizeof(stun::Header);
  size_t offset = 0;

  StunMappedAddress xor_result;
  StunMappedAddress plain_result;

  while (offset + 4 <= kAttrTotalLen) {
    auto const kAttrType = static_cast<uint16_t>(attrs[offset] << 8 | attrs[offset + 1]);
    auto const kAttrLen = static_cast<uint16_t>(attrs[offset + 2] << 8 | attrs[offset + 3]);
    offset += 4;

    if (offset + kAttrLen > kAttrTotalLen) {
      break;
    }

    if (kAttrType == stun::kAttrXorMappedAddress && kAttrLen >= 8) {
      uint8_t const kFamily = attrs[offset + 1];
      auto const kXorPort = static_cast<uint16_t>(attrs[offset + 2] << 8 | attrs[offset + 3]);
      uint16_t const kMappedPort = kXorPort ^ static_cast<uint16_t>(stun::kMagicCookie >> 16);

      if (kFamily == 0x01 && kAttrLen >= 8) {
        uint32_t xor_addr = 0;
        std::memcpy(&xor_addr, attrs + offset + 4, 4);
        uint32_t const kAddr = ntohl(xor_addr) ^ stun::kMagicCookie;
        char buf[64];
        // NOLINTNEXTLINE(cert-err33-c) — IPv4 dotted-decimal is bounded by 15 chars; buffer is generous.
        snprintf(buf, sizeof(buf), "%u.%u.%u.%u", (kAddr >> 24) & 0xFF, (kAddr >> 16) & 0xFF,
                 (kAddr >> 8) & 0xFF, kAddr & 0xFF);
        xor_result = {.ip = buf, .port = kMappedPort};
      } else if (kFamily == 0x02 && kAttrLen >= 20) {
        uint8_t addr_bytes[16];
        std::memcpy(addr_bytes, attrs + offset + 4, 16);
        uint32_t cookie = htonl(stun::kMagicCookie);
        for (int i = 0; i < 4; i++) {
          addr_bytes[i] ^= (reinterpret_cast<uint8_t*>(&cookie))[i];
        }
        for (int i = 0; i < 12; i++) {
          addr_bytes[4 + i] ^= req.transaction_id[i];
        }
        char buf[INET6_ADDRSTRLEN];
        // NOLINTNEXTLINE(cert-err33-c) — buffer is INET6_ADDRSTRLEN; the call cannot fail in this configuration.
        inet_ntop(AF_INET6, addr_bytes, buf, sizeof(buf));
        xor_result = {.ip = buf, .port = kMappedPort};
      }
    } else if (kAttrType == stun::kAttrMappedAddress && kAttrLen >= 8) {
      uint8_t const kFamily = attrs[offset + 1];
      auto const kMappedPort = static_cast<uint16_t>(attrs[offset + 2] << 8 | attrs[offset + 3]);

      if (kFamily == 0x01) {
        uint32_t addr = 0;
        std::memcpy(&addr, attrs + offset + 4, 4);
        addr = ntohl(addr);
        char buf[64];
        // NOLINTNEXTLINE(cert-err33-c) — IPv4 dotted-decimal is bounded by 15 chars; buffer is generous.
        snprintf(buf, sizeof(buf), "%u.%u.%u.%u", (addr >> 24) & 0xFF, (addr >> 16) & 0xFF,
                 (addr >> 8) & 0xFF, addr & 0xFF);
        plain_result = {.ip = buf, .port = kMappedPort};
      }
    }

    offset += (kAttrLen + 3) & ~3U;
  }

  if (!xor_result.ip.empty()) {
    return xor_result;
  }
  return plain_result;
}

// --- StunQuery (background thread, own socket) ---

void StunQuery::Start() {
  if (started_.exchange(true)) {
    return;
  }
  thread_ = std::thread(&StunQuery::Run, this);
}

void StunQuery::Start(uint16_t local_port) {
  if (started_.exchange(true)) {
    return;
  }
  localPort_ = local_port;
  thread_ = std::thread(&StunQuery::Run, this);
}

StunQuery::~StunQuery() {
  if (thread_.joinable()) {
    thread_.join();
  }
}

StunResult StunQuery::Result() const {
  std::scoped_lock const kLock(mutex_);
  return result_;
}

StunMappedAddress StunQuery::QueryServer(const char* server_addr, uint16_t port,
                                         uint16_t local_port) {
  enet_initialize();

  ENetSocket const kSock = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
  if (kSock == ENET_SOCKET_NULL) {
    return {};
  }

  if (local_port != 0) {
    enet_socket_set_option(kSock, ENET_SOCKOPT_REUSEADDR, 1);
    ENetAddress local_addr = {};
    local_addr.port = local_port;
    memset(&local_addr.host, 0, sizeof(local_addr.host));
    enet_socket_bind(kSock, &local_addr);
  }

  ENetAddress addr = {};
  addr.port = port;
  if (enet_address_set_host(&addr, server_addr) != 0) {
    enet_socket_destroy(kSock);
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
    int const kSent = enet_socket_send(kSock, &addr, &send_buf, 1);
    if (kSent < 0) {
      break;
    }

    enet_uint32 wait_condition = ENET_SOCKET_WAIT_RECEIVE;
    if (enet_socket_wait(kSock, &wait_condition, kStunTimeoutMs) != 0) {
      continue;
    }
    if (!(wait_condition & ENET_SOCKET_WAIT_RECEIVE)) {
      continue;
    }

    ENetAddress from_addr = {};
    int const kRecvLen = enet_socket_receive(kSock, &from_addr, &recv_buf, 1);
    if (std::cmp_less(kRecvLen, sizeof(stun::Header))) {
      continue;
    }

    result = stun::ParseResponse(recv_data, static_cast<size_t>(kRecvLen), req);
  }

  enet_socket_destroy(kSock);
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
    std::scoped_lock const kLock(mutex_);
    result_ = res;
  }
  done_.store(true);
}
