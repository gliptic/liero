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

static constexpr int BRIDGE_BUFSIZE = 256 * 1024;

static bool setNonBlocking(int fd) {
#ifdef _WIN32
  u_long mode = 1;
  return ioctlsocket(fd, FIONBIO, &mode) == 0;
#else
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0) return false;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

static bool setBufferSizes(int fd) {
  int sz = BRIDGE_BUFSIZE;
  setsockopt(fd, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&sz), sizeof(sz));
  setsockopt(fd, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&sz), sizeof(sz));
  return true;
}

IceBridge::~IceBridge() { destroy(); }

BridgeSocket IceBridge::create(IceAgent& agent) {
  agent_ = &agent;

  // Create two UDP sockets on localhost — must be AF_INET6 to match ENet's dual-stack sockets
  enetSocket_ = socket(AF_INET6, SOCK_DGRAM, 0);
  bridgeSocket_ = socket(AF_INET6, SOCK_DGRAM, 0);
  if (BRIDGE_INVALID_SOCKET(enetSocket_) || BRIDGE_INVALID_SOCKET(bridgeSocket_)) {
    destroy();
    return BRIDGE_INVALID;
  }

  // Disable IPV6_V6ONLY so the sockets accept IPv4-mapped addresses (matching ENet)
  int off = 0;
  setsockopt(enetSocket_, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<const char*>(&off), sizeof(off));
  setsockopt(bridgeSocket_, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<const char*>(&off), sizeof(off));

  // Bind both to IPv6 localhost (::1) with ephemeral ports
  sockaddr_in6 addrA{};
  addrA.sin6_family = AF_INET6;
  addrA.sin6_addr = in6addr_loopback;
  addrA.sin6_port = 0;

  sockaddr_in6 addrB{};
  addrB.sin6_family = AF_INET6;
  addrB.sin6_addr = in6addr_loopback;
  addrB.sin6_port = 0;

  if (bind(enetSocket_, reinterpret_cast<sockaddr*>(&addrA), sizeof(addrA)) < 0 ||
      bind(bridgeSocket_, reinterpret_cast<sockaddr*>(&addrB), sizeof(addrB)) < 0) {
    destroy();
    return -1;
  }

  // Get the assigned addresses
  socklen_t len = sizeof(addrA);
  getsockname(enetSocket_, reinterpret_cast<sockaddr*>(&addrA), &len);
  len = sizeof(addrB);
  getsockname(bridgeSocket_, reinterpret_cast<sockaddr*>(&addrB), &len);
  bridgePort_ = ntohs(addrB.sin6_port);
  enetAddr_ = addrA;
  bridgeAddr_ = addrB;

  // NOT calling connect() — ENet uses sendto() with explicit addresses,
  // which returns EISCONN on Linux if the socket is connected.

  // Configure sockets
  setNonBlocking(enetSocket_);
  setNonBlocking(bridgeSocket_);
  setBufferSizes(enetSocket_);
  setBufferSizes(bridgeSocket_);

  // Wire IceAgent's onRecv: write incoming data to enetSocket_ via bridgeSocket_
  agent_->onRecv = [this](const uint8_t* data, size_t len) {
    ::sendto(bridgeSocket_, reinterpret_cast<const char*>(data), len, 0,
             reinterpret_cast<const sockaddr*>(&enetAddr_), sizeof(enetAddr_));
  };

  return enetSocket_;
}

void IceBridge::poll() {
  if (bridgeSocket_ == BRIDGE_INVALID || !agent_) return;

  uint8_t buf[2048];
  for (;;) {
    auto n = ::recvfrom(bridgeSocket_, reinterpret_cast<char*>(buf), sizeof(buf), 0,
                        nullptr, nullptr);
    if (n <= 0) {
      if (n < 0 && BRIDGE_WOULD_BLOCK) break;
      break;
    }
    agent_->send(buf, static_cast<size_t>(n));
  }
}

void IceBridge::destroy() {
  if (agent_) {
    agent_->onRecv = nullptr;
    agent_ = nullptr;
  }
  // Don't close enetSocket_ — ownership transferred to ENet via enet_host_create.
  // ENet closes it when enet_host_destroy is called.
  enetSocket_ = BRIDGE_INVALID;
  if (bridgeSocket_ != BRIDGE_INVALID) {
    BRIDGE_CLOSE(bridgeSocket_);
    bridgeSocket_ = BRIDGE_INVALID;
  }
  bridgePort_ = 0;
}
