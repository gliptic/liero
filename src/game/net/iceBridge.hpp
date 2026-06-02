#pragma once

#include <cstdint>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using BridgeSocket = SOCKET;
static constexpr BridgeSocket kBridgeInvalid = INVALID_SOCKET;
#else
#include <netinet/in.h>
using BridgeSocket = int;
static constexpr BridgeSocket kBridgeInvalid = -1;
#endif

struct IceAgent;
struct _ENetHost;

// Loopback UDP bridge between ENet and IceAgent.
//
// Creates two AF_INET6 localhost UDP sockets (matching ENet's dual-stack):
//   - enetSocket_: given to ENet as host->socket
//   - bridgeSocket_: our side — reads ENet outbound → juice_send(),
//     and IceAgent's onRecv writes inbound → enetSocket_
//
// This lets ENet operate unmodified while libjuice handles the real network.
struct IceBridge {
  IceBridge() = default;
  ~IceBridge();

  IceBridge(const IceBridge&) = delete;
  IceBridge& operator=(const IceBridge&) = delete;

  // Create the bridge socket pair and wire up the IceAgent's onRecv callback.
  // Returns the ENet-side socket fd, or kBridgeInvalid on error.
  BridgeSocket Create(IceAgent& agent);

  // Poll: read outgoing ENet data from bridge socket → juice_send().
  // Call once per frame from the main loop.
  void Poll();

  // Get the ENet-side socket fd (for replacing host->socket after enet_host_create).
  BridgeSocket EnetSocket() const { return enetSocket_; }

  // Get the port that ENet should "connect" to (the bridge's listening port).
  uint16_t BridgePort() const { return bridgePort_; }

  // Destroy both sockets.
  void Destroy();

 private:
  BridgeSocket enetSocket_ = kBridgeInvalid;
  BridgeSocket bridgeSocket_ = kBridgeInvalid;
  uint16_t bridgePort_ = 0;
  sockaddr_in6 enetAddr_{};
  sockaddr_in6 bridgeAddr_{};
  IceAgent* agent_ = nullptr;
};
