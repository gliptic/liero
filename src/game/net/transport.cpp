#include "transport.hpp"
#include "iceAgent.hpp"

#include <cstring>
#include <cstdio>
#include <atomic>

#define ENET_IMPLEMENTATION
#include <enet.h>

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

static constexpr int NUM_CHANNELS = 2;
static constexpr int CHANNEL_RELIABLE = 0;
static constexpr int CHANNEL_UNRELIABLE = 1;

// Single active transport pointer. Only one ENet host exists per process.
static std::atomic<NetTransport*> sActiveTransport{nullptr};

static void registerTransport(_ENetHost*, NetTransport* t) {
  sActiveTransport.store(t, std::memory_order_release);
}

static void unregisterTransport(_ENetHost*) {
  sActiveTransport.store(nullptr, std::memory_order_release);
}

static NetTransport* getTransportFromHost(_ENetHost*) {
  return sActiveTransport.load(std::memory_order_acquire);
}

NetTransport::NetTransport()
    : enetHost_(nullptr), peer_(nullptr), state_(Disconnected) {
  enet_initialize();
}

NetTransport::NetTransport(NetTransport&& other) noexcept
    : enetHost_(other.enetHost_), peer_(other.peer_), state_(other.state_),
      iceBridge_(std::move(other.iceBridge_)), iceAgent_(std::move(other.iceAgent_)) {
  if (enetHost_) {
    registerTransport(enetHost_, this);
  }
  other.enetHost_ = nullptr;
  other.peer_ = nullptr;
  other.state_ = Disconnected;
}

NetTransport& NetTransport::operator=(NetTransport&& other) noexcept {
  if (this != &other) {
    disconnect();
    enetHost_ = other.enetHost_;
    peer_ = other.peer_;
    state_ = other.state_;
    iceBridge_ = std::move(other.iceBridge_);
    iceAgent_ = std::move(other.iceAgent_);
    if (enetHost_) {
      registerTransport(enetHost_, this);
    }
    other.enetHost_ = nullptr;
    other.peer_ = nullptr;
    other.state_ = Disconnected;
  }
  return *this;
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
    unregisterTransport(enetHost_);
    enet_host_destroy(enetHost_);
    enetHost_ = nullptr;
  }
  state_ = Disconnected;
}

uint16_t NetTransport::listeningPort() const {
  return enetHost_ ? enetHost_->address.port : 0;
}

bool NetTransport::createHost(uint16_t port) {
  ENetAddress address = {};
  address.port = port;

  enetHost_ = enet_host_create(&address, 1, NUM_CHANNELS, 0, 0);
  if (!enetHost_) return false;

  setupIntercept();
  return true;
}

void NetTransport::setupIntercept() {
  if (!enetHost_) return;
  registerTransport(enetHost_, this);
  enet_host_set_intercept(enetHost_, &NetTransport::interceptCallback);
}

int NetTransport::interceptCallback(_ENetHost* host, void* /*event*/) {
  NetTransport* self = getTransportFromHost(host);
  if (!self) return 0;

  uint8_t* data = host->receivedData;
  size_t len = host->receivedDataLength;

  // Let user-provided handler try (e.g., STUN responses)
  if (self->onInterceptedPacket && self->onInterceptedPacket(data, len)) {
    return 1;
  }

  return 0; // Not consumed — let ENet process it
}

bool NetTransport::host(uint16_t port) {
  if (enetHost_) return false;

  if (!createHost(port)) {
    state_ = Failed;
    return false;
  }

  state_ = Listening;
  return true;
}

bool NetTransport::connect(const std::string& address, uint16_t port) {
  if (enetHost_) return false;

  // Create host on ephemeral port
  if (!createHost(0)) {
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

bool NetTransport::connectExisting(const std::string& address, uint16_t port) {
  if (!enetHost_) return false;
  if (peer_) return false;

  ENetAddress addr = {};
  addr.port = port;
  if (enet_address_set_host(&addr, address.c_str()) != 0) {
    state_ = Failed;
    return false;
  }

  peer_ = enet_host_connect(enetHost_, &addr, NUM_CHANNELS, 0);
  if (!peer_) {
    state_ = Failed;
    return false;
  }

  state_ = Connecting;
  return true;
}

bool NetTransport::createHostOnBridgeSocket(BridgeSocket bridgeSocket) {
  if (enetHost_) return false;

  // Create ENet host with no address (won't bind a new socket)
  enetHost_ = enet_host_create(nullptr, 1, NUM_CHANNELS, 0, 0);
  if (!enetHost_) return false;

  // Replace ENet's auto-created socket with the bridge socket
  enet_socket_destroy(enetHost_->socket);
  enetHost_->socket = (ENetSocket)bridgeSocket;

  setupIntercept();
  state_ = Listening;
  return true;
}

void NetTransport::attachIce(std::unique_ptr<IceBridge> bridge, std::unique_ptr<IceAgent> agent) {
  iceBridge_ = std::move(bridge);
  iceAgent_ = std::move(agent);
}

// --- Poll ---

bool NetTransport::poll() {
  if (!enetHost_) return false;

  if (iceAgent_) iceAgent_->poll();

  ENetEvent event;
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
                if (len > 2)
                  file.assign(reinterpret_cast<const char*>(data + 2), len - 2);
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

  // Forward ENet outgoing packets through ICE bridge AFTER enet_host_service
  // (ENet sends during service, bridge picks up and forwards to libjuice)
  if (iceBridge_) iceBridge_->poll();

  return state_ == Connected || state_ == Listening || state_ == Connecting;
}

// --- Send helpers ---

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
  if (enet_peer_send(peer_, CHANNEL_RELIABLE, packet) < 0)
    enet_packet_destroy(packet);
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
  if (!levelFile.empty())
    std::memcpy(buf.data() + 2, levelFile.data(), levelFile.size());
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
  if (enet_peer_send(peer_, CHANNEL_RELIABLE, packet) < 0)
    enet_packet_destroy(packet);
}

void NetTransport::sendPacket(const void* data, size_t len) {
  if (!peer_) return;
  ENetPacket* packet =
      enet_packet_create(data, len, ENET_PACKET_FLAG_RELIABLE);
  if (!packet) return;
  if (enet_peer_send(peer_, CHANNEL_RELIABLE, packet) < 0)
    enet_packet_destroy(packet);
}
