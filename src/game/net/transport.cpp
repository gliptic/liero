#include "transport.hpp"

#include <cstring>
#include <vector>

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
  disconnect();
  enet_deinitialize();
}

void NetTransport::disconnect() {
  if (peer_) {
    enet_peer_disconnect_now(peer_, 0);
    peer_ = nullptr;
  }
  if (enetHost_) {
    enet_host_destroy(enetHost_);
    enetHost_ = nullptr;
  }
  state_ = Disconnected;
}

uint16_t NetTransport::listeningPort() const {
  return enetHost_ ? enetHost_->address.port : 0;
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

        // Defense-in-depth: reject packets larger than 10MB
        static constexpr size_t MaxPacketSize = 10 * 1024 * 1024;
        if (len > MaxPacketSize) {
          enet_packet_destroy(event.packet);
          break;
        }

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

            case PacketMapData:
              if (len > 5 && onMapData) {
                onMapData(data + 1, len - 1);
              }
              break;

            case PacketPause:
              if (onPause) onPause();
              break;

            case PacketResume:
              if (onResume) onResume();
              break;

            case PacketRematchReady:
              if (len == 2 && onRematchReady) {
                onRematchReady(data[1] != 0);
              }
              break;

            case PacketRematchLevel:
              if (len >= 2 && onRematchLevel) {
                bool random = data[1] != 0;
                std::string file;
                if (len > 2) {
                  file.assign(reinterpret_cast<const char*>(data + 2), len - 2);
                }
                onRematchLevel(random, std::move(file));
              }
              break;

            case PacketEndMatch:
              if (onEndMatch) onEndMatch();
              break;

            case PacketTcInfo:
              if (len >= 5 && onTcInfo) {
                uint32_t hash;
                std::memcpy(&hash, data + 1, 4);
                std::string name;
                if (len > 5)
                  name.assign(reinterpret_cast<const char*>(data + 5), len - 5);
                onTcInfo(hash, std::move(name));
              }
              break;

            case PacketTcResponse:
              if (len == 2 && onTcResponse) {
                onTcResponse(data[1] != 0);
              }
              break;

            case PacketTcData:
              if (len > 1 && onTcData) {
                onTcData(data + 1, len - 1);
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

void NetTransport::sendMapData(const void* data, size_t len) {
  std::vector<uint8_t> buf(1 + len);
  buf[0] = PacketMapData;
  std::memcpy(buf.data() + 1, data, len);

  if (!peer_) return;
  ENetPacket* packet =
      enet_packet_create(buf.data(), buf.size(), ENET_PACKET_FLAG_RELIABLE);
  if (!packet) return;
  if (enet_peer_send(peer_, CHANNEL_RELIABLE, packet) < 0) {
    enet_packet_destroy(packet);
  }
}

void NetTransport::sendPause() {
  uint8_t buf[1] = {PacketPause};
  sendPacket(buf, sizeof(buf));
}

void NetTransport::sendResume() {
  uint8_t buf[1] = {PacketResume};
  sendPacket(buf, sizeof(buf));
}

void NetTransport::sendRematchReady(bool ready) {
  uint8_t buf[2];
  buf[0] = PacketRematchReady;
  buf[1] = ready ? 1 : 0;
  sendPacket(buf, sizeof(buf));
}

void NetTransport::sendRematchLevel(bool randomLevel, const std::string& levelFile) {
  std::vector<uint8_t> buf(2 + levelFile.size());
  buf[0] = PacketRematchLevel;
  buf[1] = randomLevel ? 1 : 0;
  if (!levelFile.empty()) {
    std::memcpy(buf.data() + 2, levelFile.data(), levelFile.size());
  }
  sendPacket(buf.data(), buf.size());
}

void NetTransport::sendEndMatch() {
  uint8_t buf[1] = {PacketEndMatch};
  sendPacket(buf, sizeof(buf));
}

void NetTransport::sendTcInfo(uint32_t hash, const std::string& name) {
  std::vector<uint8_t> buf(1 + 4 + name.size());
  buf[0] = PacketTcInfo;
  std::memcpy(buf.data() + 1, &hash, 4);
  std::memcpy(buf.data() + 5, name.data(), name.size());
  sendPacket(buf.data(), buf.size());
}

void NetTransport::sendTcResponse(bool needData) {
  uint8_t buf[2];
  buf[0] = PacketTcResponse;
  buf[1] = needData ? 1 : 0;
  sendPacket(buf, sizeof(buf));
}

void NetTransport::sendTcData(const void* data, size_t len) {
  std::vector<uint8_t> buf(1 + len);
  buf[0] = PacketTcData;
  std::memcpy(buf.data() + 1, data, len);

  if (!peer_) return;
  ENetPacket* packet =
      enet_packet_create(buf.data(), buf.size(), ENET_PACKET_FLAG_RELIABLE);
  if (!packet) return;
  if (enet_peer_send(peer_, CHANNEL_RELIABLE, packet) < 0) {
    enet_packet_destroy(packet);
  }
}

void NetTransport::sendPacket(const void* data, size_t len) {
  if (!peer_) return;
  ENetPacket* packet =
      enet_packet_create(data, len, ENET_PACKET_FLAG_RELIABLE);
  if (!packet) return;
  if (enet_peer_send(peer_, CHANNEL_RELIABLE, packet) < 0) {
    // Send failed — ENet does NOT destroy the packet on failure
    enet_packet_destroy(packet);
  }
}
