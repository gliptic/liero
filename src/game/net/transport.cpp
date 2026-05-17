#include "transport.hpp"

#include <cstring>

#define ENET_IMPLEMENTATION
#include <enet.h>

// Channel 0: reliable ordered (inputs, handshake)
// Channel 1: unreliable (checksums — losing one is fine)
static constexpr int NUM_CHANNELS = 2;
static constexpr int CHANNEL_RELIABLE = 0;
static constexpr int CHANNEL_UNRELIABLE = 1;

NetTransport::NetTransport()
    : enetHost_(nullptr), peer_(nullptr), state_(Disconnected) {
  enet_initialize();
}

NetTransport::~NetTransport() {
  if (peer_) {
    enet_peer_disconnect_now(peer_, 0);
    peer_ = nullptr;
  }
  if (enetHost_) {
    enet_host_destroy(enetHost_);
    enetHost_ = nullptr;
  }
  enet_deinitialize();
}

bool NetTransport::host(uint16_t port) {
  if (enetHost_) return false;

  ENetAddress address = {};
  address.port = port;

  // Bind to all interfaces (IPv4 + IPv6 dual-stack)
  enetHost_ = enet_host_create(&address, 1, NUM_CHANNELS, 0, 0);
  if (!enetHost_) {
    state_ = Failed;
    return false;
  }

  state_ = Listening;
  return true;
}

bool NetTransport::connect(const std::string& address, uint16_t port) {
  if (enetHost_) return false;

  // Create client host (no incoming connections)
  enetHost_ = enet_host_create(nullptr, 1, NUM_CHANNELS, 0, 0);
  if (!enetHost_) {
    state_ = Failed;
    return false;
  }

  ENetAddress addr = {};
  addr.port = port;
  if (enet_address_set_host(&addr, address.c_str()) != 0) {
    enet_host_destroy(enetHost_);
    enetHost_ = nullptr;
    state_ = Failed;
    return false;
  }

  peer_ = enet_host_connect(enetHost_, &addr, NUM_CHANNELS, 0);
  if (!peer_) {
    enet_host_destroy(enetHost_);
    enetHost_ = nullptr;
    state_ = Failed;
    return false;
  }

  state_ = Connecting;
  return true;
}

bool NetTransport::poll() {
  if (!enetHost_) return false;

  ENetEvent event;
  // Process all pending events (timeout = 0 for non-blocking)
  while (enet_host_service(enetHost_, &event, 0) > 0) {
    switch (event.type) {
      case ENET_EVENT_TYPE_CONNECT:
        peer_ = event.peer;
        state_ = Connected;
        if (onConnected) onConnected();
        break;

      case ENET_EVENT_TYPE_RECEIVE: {
        uint8_t* data = event.packet->data;
        size_t len = event.packet->dataLength;

        if (len >= 1) {
          switch (data[0]) {
            case PacketInput:
              if (len == 6 && onRemoteInput) {
                uint32_t frame;
                std::memcpy(&frame, data + 1, 4);
                onRemoteInput(frame, data[5]);
              }
              break;

            case PacketHandshake:
              if (len == 9 && onHandshake) {
                uint32_t seed, hash;
                std::memcpy(&seed, data + 1, 4);
                std::memcpy(&hash, data + 5, 4);
                onHandshake(seed, hash);
              }
              break;

            case PacketChecksum:
              if (len == 9 && onChecksum) {
                uint32_t frame, checksum;
                std::memcpy(&frame, data + 1, 4);
                std::memcpy(&checksum, data + 5, 4);
                onChecksum(frame, checksum);
              }
              break;

            case PacketPlayerInfo:
              if (len == 1 + sizeof(PlayerInfo) && onPlayerInfo) {
                PlayerInfo info;
                std::memcpy(&info, data + 1, sizeof(PlayerInfo));
                onPlayerInfo(info);
              }
              break;

            case PacketMatchSettings:
              if (len == 1 + sizeof(MatchSettingsData) && onMatchSettings) {
                MatchSettingsData msd;
                std::memcpy(&msd, data + 1, sizeof(MatchSettingsData));
                onMatchSettings(msd);
              }
              break;
          }
        }

        enet_packet_destroy(event.packet);
        break;
      }

      case ENET_EVENT_TYPE_DISCONNECT:
      case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT:
        peer_ = nullptr;
        state_ = Disconnected;
        if (onDisconnected) onDisconnected();
        return false;

      case ENET_EVENT_TYPE_NONE:
        break;
    }
  }

  return state_ == Connected || state_ == Listening || state_ == Connecting;
}

void NetTransport::sendInput(uint32_t frame, uint8_t input) {
  uint8_t buf[6];
  buf[0] = PacketInput;
  std::memcpy(buf + 1, &frame, 4);
  buf[5] = input;
  sendPacket(buf, sizeof(buf));
}

void NetTransport::sendChecksum(uint32_t frame, uint32_t checksum) {
  uint8_t buf[9];
  buf[0] = PacketChecksum;
  std::memcpy(buf + 1, &frame, 4);
  std::memcpy(buf + 5, &checksum, 4);

  if (!peer_) return;
  ENetPacket* packet =
      enet_packet_create(buf, sizeof(buf), ENET_PACKET_FLAG_UNSEQUENCED);
  if (packet) enet_peer_send(peer_, CHANNEL_UNRELIABLE, packet);
}

void NetTransport::sendHandshake(uint32_t seed, uint32_t settingsHash) {
  uint8_t buf[9];
  buf[0] = PacketHandshake;
  std::memcpy(buf + 1, &seed, 4);
  std::memcpy(buf + 5, &settingsHash, 4);
  sendPacket(buf, sizeof(buf));
}

void NetTransport::sendPlayerInfo(const PlayerInfo& info) {
  uint8_t buf[1 + sizeof(PlayerInfo)];
  buf[0] = PacketPlayerInfo;
  std::memcpy(buf + 1, &info, sizeof(PlayerInfo));
  sendPacket(buf, sizeof(buf));
}

void NetTransport::sendMatchSettings(const MatchSettingsData& data) {
  uint8_t buf[1 + sizeof(MatchSettingsData)];
  buf[0] = PacketMatchSettings;
  std::memcpy(buf + 1, &data, sizeof(MatchSettingsData));
  sendPacket(buf, sizeof(buf));
}

void NetTransport::sendPacket(const void* data, size_t len) {
  if (!peer_) return;
  ENetPacket* packet =
      enet_packet_create(data, len, ENET_PACKET_FLAG_RELIABLE);
  if (packet) enet_peer_send(peer_, CHANNEL_RELIABLE, packet);
}
