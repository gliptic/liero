#pragma once

#include "iceBridge.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// enet.h declares these as `typedef struct _ENetHost ENetHost`; forward-
// declaring under the user-facing typedef would create a distinct
// incomplete type, so we name the underlying struct tags.
// NOLINTBEGIN(bugprone-reserved-identifier, readability-identifier-naming, cert-dcl37-c, cert-dcl51-cpp)
struct _ENetHost;
struct _ENetPeer;
using ENetHost = _ENetHost;
using ENetPeer = _ENetPeer;
// NOLINTEND(bugprone-reserved-identifier, readability-identifier-naming, cert-dcl37-c, cert-dcl51-cpp)
struct IceAgent;

// Handles UDP communication between two peers using ENet.
// Provides reliable ordered delivery of input packets.
struct NetTransport {
  // Peers running a different version handshake-mismatch and disconnect
  // rather than play with mismatched packet semantics.
  // v6: worm rgb and the level palette bytes carry 0..255 channels
  //     (previously 6-bit VGA values).
  // v7: level map blob includes display layer (display_data/display_valid).
  // v8: level map blob includes anim layer (argb_ramps/display_anim).
  static constexpr uint8_t kProtocolVersion = 8;

  // Wire sizes for hand-serialized structs (no compiler padding).
  static constexpr size_t kPlayerInfoWireSize = 5 * 4 + 4 + 3 * 4 + 24;
  static constexpr size_t kMatchSettingsWireSize = 4 * 7 + 1 + 40 * 4 + 3 + 4 * 3;

  // Packet types
  enum PacketType : uint8_t {
    kPacketInput = 1,
    kPacketHandshake = 2,
    kPacketChecksum = 3,
    kPacketPlayerInfo = 4,
    kPacketMatchSettings = 5,
    kPacketMapData = 6,
    kPacketPause = 7,
    kPacketResume = 8,
    kPacketRematchReady = 9,
    kPacketRematchLevel = 10,
    kPacketEndMatch = 11,
    kPacketTcInfo = 12,
    kPacketTcResponse = 13,
    kPacketTcData = 14,
    // K-wide redundant input window with the sender's current sim
    // frame, delivered unreliable-sequenced.
    //   [type:1][gen:1][baseFrame:u32 LE][count:u8][localDelta:u8][input[count]:u8]
    // The receiver reconstructs `remoteLocalFrame = baseFrame + localDelta`
    // for frame-advantage tracking; `gen` lets the receiver drop
    // pre-transition packets after a WS→game reset.
    kPacketInputBatch = 15,
    // Sender is leaving the match (pause menu "Disconnect"). Receiver
    // should drop back to the menu without showing stats or a
    // "peer disconnected" InfoBox.
    kPacketPeerLeft = 16,
  };

  struct PlayerInfo {
    uint32_t weapons[5];
    int32_t color;
    int32_t rgb[3];
    char name[24];
  };

  struct MatchSettingsData {
    int32_t lives;
    int32_t loading_time;
    uint32_t game_mode;
    int32_t blood;
    int32_t max_bonuses;
    int32_t time_to_lose;
    int32_t flags_to_win;
    uint8_t load_change;
    uint32_t weap_table[40];
    uint8_t regenerate_level;
    uint8_t shadow;
    uint8_t names_on_bonuses;
    int32_t blood_particle_max;
    int32_t zone_timeout;
    int32_t input_delay;
  };

  // Connection state
  enum State {
    kDisconnected,
    kListening,   // Host waiting for peer
    kConnecting,  // Client connecting to host
    kConnected,
    kFailed,
  };

  NetTransport();
  ~NetTransport();

  // Movable but not copyable (owns socket)
  NetTransport(NetTransport&& other) noexcept;
  NetTransport& operator=(NetTransport&& other) noexcept;
  NetTransport(const NetTransport&) = delete;
  NetTransport& operator=(const NetTransport&) = delete;

  void Disconnect();

  // Host a game on the given port.
  bool Host(uint16_t port);

  // Connect to a host directly.
  bool Connect(const std::string& address, uint16_t port);

  // Connect to a peer using the existing host.
  // The host must already be created.
  bool ConnectExisting(const std::string& address, uint16_t port);

  // Create ENet host using a pre-existing socket (from IceBridge).
  // The socket should be non-blocking with adequate buffer sizes.
  bool CreateHostOnBridgeSocket(BridgeSocket bridge_socket);

  // Attach ICE bridge and agent — transport takes ownership and polls them.
  // Must be called after createHostOnBridgeSocket.
  void AttachIce(std::unique_ptr<IceBridge> bridge, std::unique_ptr<IceAgent> agent);

  // --- General ---
  // Poll for events. Call once per frame.
  bool Poll();

  void SendInput(uint32_t frame, uint8_t input);
  // Rollback batched input send. `inputs` covers frames
  // [baseFrame, baseFrame + count - 1]. `localDelta` is the sender's
  // `simFrame - baseFrame` at send time (range [0, count - 1]).
  // `generation` is the sender's phase generation; receivers drop
  // older generations. Unreliable-sequenced; receiver dedups against
  // confirmed frames.
  void SendInputBatch(uint8_t generation, uint32_t base_frame, uint8_t count, uint8_t local_delta,
                      uint8_t const* inputs);
  void SendChecksum(uint8_t generation, uint32_t frame, uint32_t checksum);
  void SendHandshake(uint32_t seed, uint32_t settings_hash);
  void SendPlayerInfo(const PlayerInfo& info);
  void SendMatchSettings(const MatchSettingsData& data);
  void SendMapData(const void* data, size_t len);
  void SendPause();
  void SendResume();
  void SendRematchReady(bool ready);
  void SendRematchLevel(bool random_level, const std::string& level_file);
  void SendEndMatch();
  void SendPeerLeft();
  void SendTcInfo(uint32_t hash, const std::string& name);
  void SendTcResponse(bool need_data);
  void SendTcData(const void* data, size_t len);

  State CurrentState() const { return state_; }
  uint16_t ListeningPort() const;

  // Access the ENet host (for STUN-via-host integration)
  ENetHost* EnetHost() const { return enetHost_; }

  // Callbacks
  std::function<void(uint32_t frame, uint8_t input)> on_remote_input;
  // Rollback batched input arrival. `remoteLocalFrame` = sender's
  // simFrame at send time (= baseFrame + localDelta) — used for
  // frame-advantage tracking. `inputs` is valid only for the duration
  // of the callback; copy if you need to keep it.
  std::function<void(uint8_t generation, uint32_t base_frame, uint8_t count, uint8_t const* inputs,
                     uint32_t remote_local_frame)>
      on_remote_input_batch;
  std::function<void(uint32_t seed, uint32_t settings_hash)> on_handshake;
  std::function<void(uint8_t generation, uint32_t frame, uint32_t checksum)> on_checksum;
  std::function<void(const PlayerInfo& info)> on_player_info;
  std::function<void(const MatchSettingsData& data)> on_match_settings;
  std::function<void(const void* data, size_t len)> on_map_data;
  std::function<void()> on_pause;
  std::function<void()> on_resume;
  std::function<void(bool ready)> on_rematch_ready;
  std::function<void(bool random_level, std::string level_file)> on_rematch_level;
  std::function<void()> on_end_match;
  std::function<void()> on_peer_left;
  std::function<void(uint32_t hash, std::string name)> on_tc_info;
  std::function<void(bool need_data)> on_tc_response;
  std::function<void(const void* data, size_t len)> on_tc_data;
  std::function<void()> on_connected;
  std::function<void()> on_disconnected;
  // Called for each non-ENet packet intercepted (STUN, etc.)
  // Return true if consumed.
  std::function<bool(const uint8_t* data, size_t len)> on_intercepted_packet;

 private:
  void SendPacket(const void* data, size_t len);
  bool CreateHost(uint16_t port);
  void SetupIntercept();

  static int InterceptCallback(ENetHost* host, void* event);

  ENetHost* enetHost_{nullptr};
  ENetPeer* peer_{nullptr};
  enum State state_ { kDisconnected };
  std::unique_ptr<IceBridge> iceBridge_;
  std::unique_ptr<IceAgent> iceAgent_;
};
