#pragma once

#include <cstdint>
#include <functional>
#include <string>

struct _ENetHost;
struct _ENetPeer;

// Handles UDP communication between two peers using ENet.
// Provides reliable ordered delivery of input packets.
struct NetTransport {
  // Packet types
  enum PacketType : uint8_t {
    PacketInput = 1,      // Frame input: [type(1) | frame(4) | input(1)]
    PacketHandshake = 2,  // Initial sync: [type(1) | seed(4) | settingsHash(4)]
    PacketChecksum = 3,   // Desync check: [type(1) | frame(4) | checksum(4)]
  };

  // Connection state
  enum State {
    Disconnected,
    Listening,     // Host waiting for peer
    Connecting,    // Client connecting to host
    Connected,
    Failed,
  };

  NetTransport();
  ~NetTransport();

  // Host a game on the given port. Returns true if listening started.
  bool host(uint16_t port);

  // Connect to a host. Returns true if connection attempt started.
  bool connect(const std::string& address, uint16_t port);

  // Poll for events and deliver received inputs. Call once per frame.
  // Returns true if still connected.
  bool poll();

  // Send local input for a given frame
  void sendInput(uint32_t frame, uint8_t input);

  // Send a checksum for desync detection
  void sendChecksum(uint32_t frame, uint32_t checksum);

  // Send handshake (seed + settings hash)
  void sendHandshake(uint32_t seed, uint32_t settingsHash);

  State state() const { return state_; }

  // Callbacks set by the controller
  std::function<void(uint32_t frame, uint8_t input)> onRemoteInput;
  std::function<void(uint32_t seed, uint32_t settingsHash)> onHandshake;
  std::function<void(uint32_t frame, uint32_t checksum)> onChecksum;
  std::function<void()> onConnected;
  std::function<void()> onDisconnected;

 private:
  void sendPacket(const void* data, size_t len);

  _ENetHost* enetHost_;
  _ENetPeer* peer_;
  State state_;
};
