#include "transport.hpp"
#include "iceAgent.hpp"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define ENET_IMPLEMENTATION
#include <enet.h>

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

static void WriteU32(uint8_t* p, uint32_t v) { std::memcpy(p, &v, 4); }
static void WriteI32(uint8_t* p, int32_t v) { std::memcpy(p, &v, 4); }
static uint32_t ReadU32(const uint8_t* p) {
  uint32_t v = 0;
  std::memcpy(&v, p, 4);
  return v;
}
static int32_t ReadI32(const uint8_t* p) {
  int32_t v = 0;
  std::memcpy(&v, p, 4);
  return v;
}

static constexpr int kNumChannels = 3;
static constexpr int kChannelReliable = 0;
static constexpr int kChannelUnreliable = 1;
static constexpr int kChannelInputBatch = 2;

// Single active transport pointer. Only one ENet host exists per process.
static std::atomic<NetTransport*> s_active_transport{nullptr};

static void RegisterTransport(_ENetHost* /*unused*/, NetTransport* t) {
  s_active_transport.store(t, std::memory_order_release);
}

static void UnregisterTransport(_ENetHost* /*unused*/) {
  s_active_transport.store(nullptr, std::memory_order_release);
}

static NetTransport* GetTransportFromHost(_ENetHost* /*unused*/) {
  return s_active_transport.load(std::memory_order_acquire);
}

NetTransport::NetTransport() { enet_initialize(); }

NetTransport::NetTransport(NetTransport&& other) noexcept
    : enetHost_(other.enetHost_),
      peer_(other.peer_),
      state_(other.state_),
      iceBridge_(std::move(other.iceBridge_)),
      iceAgent_(std::move(other.iceAgent_)) {
  if (enetHost_) {
    RegisterTransport(enetHost_, this);
  }
  other.enetHost_ = nullptr;
  other.peer_ = nullptr;
  other.state_ = kDisconnected;
}

NetTransport& NetTransport::operator=(NetTransport&& other) noexcept {
  if (this != &other) {
    Disconnect();
    enetHost_ = other.enetHost_;
    peer_ = other.peer_;
    state_ = other.state_;
    iceBridge_ = std::move(other.iceBridge_);
    iceAgent_ = std::move(other.iceAgent_);
    if (enetHost_) {
      RegisterTransport(enetHost_, this);
    }
    other.enetHost_ = nullptr;
    other.peer_ = nullptr;
    other.state_ = kDisconnected;
  }
  return *this;
}

NetTransport::~NetTransport() {
  Disconnect();
  enet_deinitialize();
}

void NetTransport::Disconnect() {
  if (peer_) {
    enet_peer_disconnect_now(peer_, 0);
    peer_ = nullptr;
  }
  if (enetHost_) {
    UnregisterTransport(enetHost_);
    enet_host_destroy(enetHost_);
    enetHost_ = nullptr;
  }
  state_ = kDisconnected;
}

uint16_t NetTransport::ListeningPort() const { return enetHost_ ? enetHost_->address.port : 0; }

bool NetTransport::CreateHost(uint16_t port) {
  ENetAddress address = {};
  address.port = port;

  enetHost_ = enet_host_create(&address, 1, kNumChannels, 0, 0);
  if (!enetHost_) return false;

  SetupIntercept();
  return true;
}

void NetTransport::SetupIntercept() {
  if (!enetHost_) return;
  RegisterTransport(enetHost_, this);
  enet_host_set_intercept(enetHost_, &NetTransport::InterceptCallback);
}

int NetTransport::InterceptCallback(_ENetHost* host, void* /*event*/) {
  NetTransport const* self = GetTransportFromHost(host);
  if (!self) return 0;

  uint8_t const* data = host->receivedData;
  size_t const kLen = host->receivedDataLength;

  // Let user-provided handler try (e.g., STUN responses)
  if (self->on_intercepted_packet && self->on_intercepted_packet(data, kLen)) {
    return 1;
  }

  return 0;  // Not consumed — let ENet process it
}

bool NetTransport::Host(uint16_t port) {
  if (enetHost_) return false;

  if (!CreateHost(port)) {
    state_ = kFailed;
    return false;
  }

  state_ = kListening;
  return true;
}

bool NetTransport::Connect(const std::string& address, uint16_t port) {
  if (enetHost_) return false;

  // Create host on ephemeral port
  if (!CreateHost(0)) {
    state_ = kFailed;
    return false;
  }

  ENetAddress addr = {};
  addr.port = port;
  if (enet_address_set_host(&addr, address.c_str()) != 0) {
    enet_host_destroy(enetHost_);
    enetHost_ = nullptr;
    state_ = kFailed;
    return false;
  }

  peer_ = enet_host_connect(enetHost_, &addr, kNumChannels, 0);
  if (!peer_) {
    enet_host_destroy(enetHost_);
    enetHost_ = nullptr;
    state_ = kFailed;
    return false;
  }

  state_ = kConnecting;
  return true;
}

bool NetTransport::ConnectExisting(const std::string& address, uint16_t port) {
  if (!enetHost_) return false;
  if (peer_) return false;

  ENetAddress addr = {};
  addr.port = port;
  if (enet_address_set_host(&addr, address.c_str()) != 0) {
    state_ = kFailed;
    return false;
  }

  peer_ = enet_host_connect(enetHost_, &addr, kNumChannels, 0);
  if (!peer_) {
    state_ = kFailed;
    return false;
  }

  state_ = kConnecting;
  return true;
}

bool NetTransport::CreateHostOnBridgeSocket(BridgeSocket bridge_socket) {
  if (enetHost_) return false;

  // Create ENet host with no address (won't bind a new socket)
  enetHost_ = enet_host_create(nullptr, 1, kNumChannels, 0, 0);
  if (!enetHost_) return false;

  // Replace ENet's auto-created socket with the bridge socket
  enet_socket_destroy(enetHost_->socket);
  enetHost_->socket = static_cast<ENetSocket>(bridge_socket);

  SetupIntercept();
  state_ = kListening;
  return true;
}

void NetTransport::AttachIce(std::unique_ptr<IceBridge> bridge, std::unique_ptr<IceAgent> agent) {
  iceBridge_ = std::move(bridge);
  iceAgent_ = std::move(agent);
}

// --- Poll ---

bool NetTransport::Poll() {
  if (!enetHost_) return false;

  if (iceAgent_) iceAgent_->Poll();

  ENetEvent event;
  while (enet_host_service(enetHost_, &event, 0) > 0) {
    switch (event.type) {
      case ENET_EVENT_TYPE_CONNECT:
        peer_ = event.peer;
        state_ = kConnected;
        if (on_connected) on_connected();
        break;

      case ENET_EVENT_TYPE_RECEIVE: {
        uint8_t const* data = event.packet->data;
        size_t const kLen = event.packet->dataLength;

        static constexpr size_t kMaxPacketSize = 10 * 1024 * 1024;
        if (kLen > kMaxPacketSize) {
          enet_packet_destroy(event.packet);
          break;
        }

        // Behind OPENLIERO_CHECKSUM_LOG=1: count received packet types
        // so we can tell whether checksum packets are reaching the wire
        // at all (vs being lost in the ICE bridge or never sent).
        if (kLen >= 1) {
          static const bool kLogEnabled = []() {
            char const* e = std::getenv("OPENLIERO_CHECKSUM_LOG");
            return e && *e && *e != '0';
          }();
          if (kLogEnabled) {
            static uint64_t cnt_input = 0;
            static uint64_t cnt_batch = 0;
            static uint64_t cnt_checksum = 0;
            static uint64_t cnt_other = 0;
            switch (data[0]) {
              case kPacketInput:
                ++cnt_input;
                break;
              case kPacketInputBatch:
                ++cnt_batch;
                break;
              case kPacketChecksum:
                ++cnt_checksum;
                break;
              default:
                ++cnt_other;
                break;
            }
            uint64_t const kTotal = cnt_input + cnt_batch + cnt_checksum + cnt_other;
            if (kTotal > 0 && kTotal % 140 == 0) {
              std::fprintf(stderr,
                           "[transport rx] input=%llu batch=%llu checksum=%llu other=%llu\n",
                           static_cast<unsigned long long>(cnt_input),
                           static_cast<unsigned long long>(cnt_batch),
                           static_cast<unsigned long long>(cnt_checksum),
                           static_cast<unsigned long long>(cnt_other));
            }
          }
        }

        if (kLen >= 1) {
          switch (data[0]) {
            case kPacketInput:
              if (kLen == 6 && on_remote_input) {
                uint32_t frame = 0;
                std::memcpy(&frame, data + 1, 4);
                on_remote_input(frame, data[5]);
              }
              break;
            case kPacketInputBatch:
              // [type:1][gen:1][baseFrame:u32 LE][count:u8][localDelta:u8]
              // [input[count]:u8]. Validate that localDelta is in range
              // (< count) and that the payload length matches count
              // exactly — anything else is malformed or a version drift.
              if (kLen >= 8 && on_remote_input_batch) {
                uint8_t const kGen = data[1];
                uint32_t base_frame = 0;
                std::memcpy(&base_frame, data + 2, 4);
                uint8_t const kCount = data[6];
                uint8_t const kLocalDelta = data[7];
                if (kCount == 0 || kLen != size_t{8} + kCount) break;
                if (kLocalDelta >= kCount) break;
                uint32_t const kRemoteLocalFrame = base_frame + kLocalDelta;
                on_remote_input_batch(kGen, base_frame, kCount, data + 8, kRemoteLocalFrame);
              }
              break;
            case kPacketHandshake:
              // [type:1][version:1][seed:4][hash:4] = 10 B.
              if (kLen == 10 && on_handshake) {
                if (data[1] != kProtocolVersion) {
                  // Loud on stderr: a silent drop here surfaces as the
                  // session sitting in Handshaking forever, which is
                  // hard to attribute to a version mismatch. Surface
                  // the actual cause so mixed-version test setups are
                  // diagnosable immediately.
                  std::fprintf(stderr,
                               "[transport] handshake protocol version mismatch: peer=%u local=%u "
                               "— peers must be on the same build\n",
                               static_cast<unsigned>(data[1]),
                               static_cast<unsigned>(kProtocolVersion));
                  break;
                }
                uint32_t seed = 0;
                uint32_t hash = 0;
                std::memcpy(&seed, data + 2, 4);
                std::memcpy(&hash, data + 6, 4);
                on_handshake(seed, hash);
              }
              break;
            case kPacketChecksum:
              // [type:1][gen:1][frame:u32 LE][checksum:u32 LE] = 10 B.
              if (kLen == 10 && on_checksum) {
                uint8_t const kGen = data[1];
                uint32_t frame = 0;
                uint32_t checksum = 0;
                std::memcpy(&frame, data + 2, 4);
                std::memcpy(&checksum, data + 6, 4);
                on_checksum(kGen, frame, checksum);
              }
              break;
            case kPacketPlayerInfo:
              if (kLen == 1 + kPlayerInfoWireSize && on_player_info) {
                PlayerInfo info{};
                const uint8_t* p = data + 1;
                for (int i = 0; i < 5; ++i, p += 4) info.weapons[i] = ReadU32(p);
                info.color = ReadI32(p);
                p += 4;
                for (int i = 0; i < 3; ++i, p += 4) info.rgb[i] = ReadI32(p);
                std::memcpy(info.name, p, 24);
                on_player_info(info);
              }
              break;
            case kPacketMatchSettings:
              if (kLen == 1 + kMatchSettingsWireSize && on_match_settings) {
                MatchSettingsData msd{};
                const uint8_t* p = data + 1;
                msd.lives = ReadI32(p);
                p += 4;
                msd.loading_time = ReadI32(p);
                p += 4;
                msd.game_mode = ReadU32(p);
                p += 4;
                msd.blood = ReadI32(p);
                p += 4;
                msd.max_bonuses = ReadI32(p);
                p += 4;
                msd.time_to_lose = ReadI32(p);
                p += 4;
                msd.flags_to_win = ReadI32(p);
                p += 4;
                msd.load_change = *p++;
                for (int i = 0; i < 40; ++i, p += 4) msd.weap_table[i] = ReadU32(p);
                msd.regenerate_level = *p++;
                msd.shadow = *p++;
                msd.names_on_bonuses = *p++;
                msd.blood_particle_max = ReadI32(p);
                p += 4;
                msd.zone_timeout = ReadI32(p);
                p += 4;
                msd.input_delay = ReadI32(p);
                on_match_settings(msd);
              }
              break;
            case kPacketMapData:
              if (kLen > 5 && on_map_data) {
                on_map_data(data + 1, kLen - 1);
              }
              break;
            case kPacketPause:
              if (on_pause) on_pause();
              break;
            case kPacketResume:
              if (on_resume) on_resume();
              break;
            case kPacketRematchReady:
              if (kLen == 2 && on_rematch_ready) {
                on_rematch_ready(data[1] != 0);
              }
              break;
            case kPacketRematchLevel:
              if (kLen >= 2 && on_rematch_level) {
                bool const kRandom = data[1] != 0;
                std::string file;
                if (kLen > 2) file.assign(reinterpret_cast<const char*>(data + 2), kLen - 2);
                on_rematch_level(kRandom, std::move(file));
              }
              break;
            case kPacketEndMatch:
              if (on_end_match) on_end_match();
              break;
            case kPacketPeerLeft:
              if (on_peer_left) on_peer_left();
              break;
            case kPacketTcInfo:
              if (kLen >= 5 && on_tc_info) {
                uint32_t hash = 0;
                std::memcpy(&hash, data + 1, 4);
                std::string name;
                if (kLen > 5) name.assign(reinterpret_cast<const char*>(data + 5), kLen - 5);
                on_tc_info(hash, std::move(name));
              }
              break;
            case kPacketTcResponse:
              if (kLen == 2 && on_tc_response) {
                on_tc_response(data[1] != 0);
              }
              break;
            case kPacketTcData:
              if (kLen > 1 && on_tc_data) {
                on_tc_data(data + 1, kLen - 1);
              }
              break;
            default:
              break;
          }
        }

        enet_packet_destroy(event.packet);
        break;
      }

      case ENET_EVENT_TYPE_DISCONNECT:
      case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT:
        peer_ = nullptr;
        state_ = kDisconnected;
        if (on_disconnected) on_disconnected();
        return false;

      case ENET_EVENT_TYPE_NONE:
        break;
    }
  }

  // Forward ENet outgoing packets through ICE bridge AFTER enet_host_service
  // (ENet sends during service, bridge picks up and forwards to libjuice)
  if (iceBridge_) iceBridge_->Poll();

  return state_ == kConnected || state_ == kListening || state_ == kConnecting;
}

// --- Send helpers ---

void NetTransport::SendInput(uint32_t frame, uint8_t input) {
  uint8_t buf[6];
  buf[0] = kPacketInput;
  std::memcpy(buf + 1, &frame, 4);
  buf[5] = input;
  SendPacket(buf, sizeof(buf));
}

void NetTransport::SendInputBatch(uint8_t generation, uint32_t base_frame, uint8_t count,
                                  uint8_t local_delta, uint8_t const* inputs) {
  if (!peer_ || count == 0 || local_delta >= count) return;
  // Cap to a defensive maximum so a misuse can't blow the stack
  // buffer. Rollback uses count = kMaxRollback + 1 (= 8); accept
  // anything up to 64 for headroom.
  static constexpr uint8_t kMaxCount = 64;
  if (count > kMaxCount) return;

  uint8_t buf[8 + kMaxCount];
  buf[0] = kPacketInputBatch;
  buf[1] = generation;
  std::memcpy(buf + 2, &base_frame, 4);
  buf[6] = count;
  buf[7] = local_delta;
  std::memcpy(buf + 8, inputs, count);

  size_t const kLen = size_t{8} + count;
  // UNSEQUENCED, not 0: sequenced-discard would drop an older batch
  // delivered after a newer one. injectRemoteInput already dedups.
  ENetPacket* packet = enet_packet_create(buf, kLen, ENET_PACKET_FLAG_UNSEQUENCED);
  if (!packet) return;
  if (enet_peer_send(peer_, kChannelInputBatch, packet) < 0) enet_packet_destroy(packet);
}

void NetTransport::SendChecksum(uint8_t generation, uint32_t frame, uint32_t checksum) {
  uint8_t buf[10];
  buf[0] = kPacketChecksum;
  buf[1] = generation;
  std::memcpy(buf + 2, &frame, 4);
  std::memcpy(buf + 6, &checksum, 4);

  if (!peer_) return;
  ENetPacket* packet = enet_packet_create(buf, sizeof(buf), ENET_PACKET_FLAG_UNSEQUENCED);
  if (packet) enet_peer_send(peer_, kChannelUnreliable, packet);
}

void NetTransport::SendHandshake(uint32_t seed, uint32_t settings_hash) {
  uint8_t buf[10];
  buf[0] = kPacketHandshake;
  buf[1] = kProtocolVersion;
  std::memcpy(buf + 2, &seed, 4);
  std::memcpy(buf + 6, &settings_hash, 4);
  SendPacket(buf, sizeof(buf));
}

void NetTransport::SendPlayerInfo(const PlayerInfo& info) {
  uint8_t buf[1 + kPlayerInfoWireSize];
  buf[0] = kPacketPlayerInfo;
  uint8_t* p = buf + 1;
  for (int i = 0; i < 5; ++i, p += 4) WriteU32(p, info.weapons[i]);
  WriteI32(p, info.color);
  p += 4;
  for (int i = 0; i < 3; ++i, p += 4) WriteI32(p, info.rgb[i]);
  std::memcpy(p, info.name, 24);
  SendPacket(buf, sizeof(buf));
}

void NetTransport::SendMatchSettings(const MatchSettingsData& data) {
  uint8_t buf[1 + kMatchSettingsWireSize];
  buf[0] = kPacketMatchSettings;
  uint8_t* p = buf + 1;
  WriteI32(p, data.lives);
  p += 4;
  WriteI32(p, data.loading_time);
  p += 4;
  WriteU32(p, data.game_mode);
  p += 4;
  WriteI32(p, data.blood);
  p += 4;
  WriteI32(p, data.max_bonuses);
  p += 4;
  WriteI32(p, data.time_to_lose);
  p += 4;
  WriteI32(p, data.flags_to_win);
  p += 4;
  *p++ = data.load_change;
  for (int i = 0; i < 40; ++i, p += 4) WriteU32(p, data.weap_table[i]);
  *p++ = data.regenerate_level;
  *p++ = data.shadow;
  *p++ = data.names_on_bonuses;
  WriteI32(p, data.blood_particle_max);
  p += 4;
  WriteI32(p, data.zone_timeout);
  p += 4;
  WriteI32(p, data.input_delay);
  SendPacket(buf, sizeof(buf));
}

void NetTransport::SendMapData(const void* data, size_t len) {
  std::vector<uint8_t> buf(1 + len);
  buf[0] = kPacketMapData;
  std::memcpy(buf.data() + 1, data, len);

  if (!peer_) return;
  ENetPacket* packet = enet_packet_create(buf.data(), buf.size(), ENET_PACKET_FLAG_RELIABLE);
  if (!packet) return;
  if (enet_peer_send(peer_, kChannelReliable, packet) < 0) enet_packet_destroy(packet);
}

void NetTransport::SendPause() {
  uint8_t buf[1] = {kPacketPause};
  SendPacket(buf, sizeof(buf));
}

void NetTransport::SendResume() {
  uint8_t buf[1] = {kPacketResume};
  SendPacket(buf, sizeof(buf));
}

void NetTransport::SendRematchReady(bool ready) {
  uint8_t buf[2];
  buf[0] = kPacketRematchReady;
  buf[1] = ready ? 1 : 0;
  SendPacket(buf, sizeof(buf));
}

void NetTransport::SendRematchLevel(bool random_level, const std::string& level_file) {
  std::vector<uint8_t> buf(2 + level_file.size());
  buf[0] = kPacketRematchLevel;
  buf[1] = random_level ? 1 : 0;
  if (!level_file.empty()) std::memcpy(buf.data() + 2, level_file.data(), level_file.size());
  SendPacket(buf.data(), buf.size());
}

void NetTransport::SendEndMatch() {
  uint8_t buf[1] = {kPacketEndMatch};
  SendPacket(buf, sizeof(buf));
}

void NetTransport::SendPeerLeft() {
  uint8_t buf[1] = {kPacketPeerLeft};
  SendPacket(buf, sizeof(buf));
}

void NetTransport::SendTcInfo(uint32_t hash, const std::string& name) {
  std::vector<uint8_t> buf(1 + 4 + name.size());
  buf[0] = kPacketTcInfo;
  std::memcpy(buf.data() + 1, &hash, 4);
  std::memcpy(buf.data() + 5, name.data(), name.size());
  SendPacket(buf.data(), buf.size());
}

void NetTransport::SendTcResponse(bool need_data) {
  uint8_t buf[2];
  buf[0] = kPacketTcResponse;
  buf[1] = need_data ? 1 : 0;
  SendPacket(buf, sizeof(buf));
}

void NetTransport::SendTcData(const void* data, size_t len) {
  std::vector<uint8_t> buf(1 + len);
  buf[0] = kPacketTcData;
  std::memcpy(buf.data() + 1, data, len);

  if (!peer_) return;
  ENetPacket* packet = enet_packet_create(buf.data(), buf.size(), ENET_PACKET_FLAG_RELIABLE);
  if (!packet) return;
  if (enet_peer_send(peer_, kChannelReliable, packet) < 0) enet_packet_destroy(packet);
}

void NetTransport::SendPacket(const void* data, size_t len) {
  if (!peer_) return;
  ENetPacket* packet = enet_packet_create(data, len, ENET_PACKET_FLAG_RELIABLE);
  if (!packet) return;
  if (enet_peer_send(peer_, kChannelReliable, packet) < 0) enet_packet_destroy(packet);
}
