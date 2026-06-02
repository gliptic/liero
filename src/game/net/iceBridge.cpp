#include "iceBridge.hpp"
#include "iceAgent.hpp"

#include <cerrno>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define BRIDGE_CLOSE closesocket
#define BRIDGE_WOULD_BLOCK (WSAGetLastError() == WSAEWOULDBLOCK)
#define BRIDGE_INVALID_SOCKET(s) ((s) == INVALID_SOCKET)
using socklen_t = int;
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#define BRIDGE_CLOSE close
#define BRIDGE_WOULD_BLOCK (errno == EAGAIN || errno == EWOULDBLOCK)
#define BRIDGE_INVALID_SOCKET(s) ((s) < 0)
#endif

static constexpr int kBridgeBufsize = 256 * 1024;

static bool SetNonBlocking(int fd) {
#ifdef _WIN32
  u_long mode = 1;
  return ioctlsocket(fd, FIONBIO, &mode) == 0;
#else
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0) return false;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

static bool SetBufferSizes(int fd) {
  int sz = kBridgeBufsize;
  setsockopt(fd, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&sz), sizeof(sz));
  setsockopt(fd, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&sz), sizeof(sz));
  return true;
}

IceBridge::~IceBridge() { Destroy(); }

BridgeSocket IceBridge::Create(IceAgent& agent) {
  agent_ = &agent;

  // Create two UDP sockets on localhost — must be AF_INET6 to match ENet's dual-stack sockets
  enetSocket_ = socket(AF_INET6, SOCK_DGRAM, 0);
  bridgeSocket_ = socket(AF_INET6, SOCK_DGRAM, 0);
  if (BRIDGE_INVALID_SOCKET(enetSocket_) || BRIDGE_INVALID_SOCKET(bridgeSocket_)) {
    Destroy();
    return kBridgeInvalid;
  }

  // Disable IPV6_V6ONLY so the sockets accept IPv4-mapped addresses (matching ENet)
  int off = 0;
  setsockopt(enetSocket_, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<const char*>(&off),
             sizeof(off));
  setsockopt(bridgeSocket_, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<const char*>(&off),
             sizeof(off));

  // Bind both to IPv6 localhost (::1) with ephemeral ports
  sockaddr_in6 addr_a{};
  addr_a.sin6_family = AF_INET6;
  addr_a.sin6_addr = in6addr_loopback;
  addr_a.sin6_port = 0;

  sockaddr_in6 addr_b{};
  addr_b.sin6_family = AF_INET6;
  addr_b.sin6_addr = in6addr_loopback;
  addr_b.sin6_port = 0;

  if (bind(enetSocket_, reinterpret_cast<sockaddr*>(&addr_a), sizeof(addr_a)) < 0 ||
      bind(bridgeSocket_, reinterpret_cast<sockaddr*>(&addr_b), sizeof(addr_b)) < 0) {
    Destroy();
    return -1;
  }

  // Get the assigned addresses
  socklen_t len = sizeof(addr_a);
  getsockname(enetSocket_, reinterpret_cast<sockaddr*>(&addr_a), &len);
  len = sizeof(addr_b);
  getsockname(bridgeSocket_, reinterpret_cast<sockaddr*>(&addr_b), &len);
  bridgePort_ = ntohs(addr_b.sin6_port);
  enetAddr_ = addr_a;
  bridgeAddr_ = addr_b;

  // NOT calling connect() — ENet uses sendto() with explicit addresses,
  // which returns EISCONN on Linux if the socket is connected.

  // Configure sockets
  SetNonBlocking(enetSocket_);
  SetNonBlocking(bridgeSocket_);
  SetBufferSizes(enetSocket_);
  SetBufferSizes(bridgeSocket_);

  // Wire IceAgent's onRecv: write incoming data to enetSocket_ via bridgeSocket_
  agent_->on_recv = [this](const uint8_t* data, size_t len) {
    ::sendto(bridgeSocket_, reinterpret_cast<const char*>(data), len, 0,
             reinterpret_cast<const sockaddr*>(&enetAddr_), sizeof(enetAddr_));
  };

  return enetSocket_;
}

void IceBridge::Poll() {
  if (bridgeSocket_ == kBridgeInvalid || !agent_) return;

  uint8_t buf[2048];
  for (;;) {
    auto n =
        ::recvfrom(bridgeSocket_, reinterpret_cast<char*>(buf), sizeof(buf), 0, nullptr, nullptr);
    if (n <= 0) {
      if (n < 0 && BRIDGE_WOULD_BLOCK) break;
      break;
    }
    agent_->Send(buf, static_cast<size_t>(n));
  }
}

void IceBridge::Destroy() {
  if (agent_) {
    agent_->on_recv = nullptr;
    agent_ = nullptr;
  }
  // Don't close enetSocket_ — ownership transferred to ENet via enet_host_create.
  // ENet closes it when enet_host_destroy is called.
  enetSocket_ = kBridgeInvalid;
  if (bridgeSocket_ != kBridgeInvalid) {
    BRIDGE_CLOSE(bridgeSocket_);
    bridgeSocket_ = kBridgeInvalid;
  }
  bridgePort_ = 0;
}
