#pragma once

#include "iceBridge.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct _ENetHost;
struct _ENetPeer;
struct IceAgent;

// Handles UDP communication between two peers using ENet.
// Provides reliable ordered delivery of input packets.
struct NetTransport {
  // Peers running a different version handshake-mismatch and disconnect
  // rather than play with mismatched packet semantics.
  static constexpr uint8_t kProtocolVersion = 5;

  // Wire sizes for hand-serialized structs (no compiler padding).
  static constexpr size_t kPlayerInfoWireSize = 5 * 4 + 4 + 3 * 4 + 24;
  static constexpr size_t kMatchSettingsWireSize = 4 * 7 + 1 + 40 * 4 + 3 + 4 * 3;

  // Packet types
  enum PacketType : uint8_t {
    PacketInput = 1,
    PacketHandshake = 2,
    PacketChecksum = 3,
    PacketPlayerInfo = 4,
    PacketMatchSettings = 5,
    PacketMapData = 6,
    PacketPause = 7,
    PacketResume = 8,
    PacketRematchReady = 9,
    PacketRematchLevel = 10,
    PacketEndMatch = 11,
    PacketTcInfo = 12,
    PacketTcResponse = 13,
    PacketTcData = 14,
    // K-wide redundant input window with the sender's current sim
    // frame, delivered unreliable-sequenced.
    //   [type:1][gen:1][baseFrame:u32 LE][count:u8][localDelta:u8][input[count]:u8]
    // The receiver reconstructs `remoteLocalFrame = baseFrame + localDelta`
    // for frame-advantage tracking; `gen` lets the receiver drop
    // pre-transition packets after a WS→game reset.
    PacketInputBatch = 15,
    // Sender is leaving the match (pause menu "Disconnect"). Receiver
    // should drop back to the menu without showing stats or a
    // "peer disconnected" InfoBox.
    PacketPeerLeft = 16,
  };

  struct PlayerInfo {
    uint32_t weapons[5];
    int32_t color;
    int32_t rgb[3];
    char name[24];
  };

  struct MatchSettingsData {
    int32_t lives;
    int32_t loadingTime;
    uint32_t gameMode;
    int32_t blood;
    int32_t maxBonuses;
    int32_t timeToLose;
    int32_t flagsToWin;
    uint8_t loadChange;
    uint32_t weapTable[40];
    uint8_t regenerateLevel;
    uint8_t shadow;
    uint8_t namesOnBonuses;
    int32_t bloodParticleMax;
    int32_t zoneTimeout;
    int32_t inputDelay;
  };

  // Connection state
  enum State {
    Disconnected,
    Listening,   // Host waiting for peer
    Connecting,  // Client connecting to host
    Connected,
    Failed,
  };

  NetTransport();
  ~NetTransport();

  // Movable but not copyable (owns socket)
  NetTransport(NetTransport&& other) noexcept;
  NetTransport& operator=(NetTransport&& other) noexcept;
  NetTransport(const NetTransport&) = delete;
  NetTransport& operator=(const NetTransport&) = delete;

  void disconnect();

  // Host a game on the given port.
  bool host(uint16_t port);

  // Connect to a host directly.
  bool connect(const std::string& address, uint16_t port);

  // Connect to a peer using the existing host.
  // The host must already be created.
  bool connectExisting(const std::string& address, uint16_t port);

  // Create ENet host using a pre-existing socket (from IceBridge).
  // The socket should be non-blocking with adequate buffer sizes.
  bool createHostOnBridgeSocket(BridgeSocket bridgeSocket);

  // Attach ICE bridge and agent — transport takes ownership and polls them.
  // Must be called after createHostOnBridgeSocket.
  void attachIce(std::unique_ptr<IceBridge> bridge, std::unique_ptr<IceAgent> agent);

  // --- General ---
  // Poll for events. Call once per frame.
  bool poll();

  void sendInput(uint32_t frame, uint8_t input);
  // Rollback batched input send. `inputs` covers frames
  // [baseFrame, baseFrame + count - 1]. `localDelta` is the sender's
  // `simFrame - baseFrame` at send time (range [0, count - 1]).
  // `generation` is the sender's phase generation; receivers drop
  // older generations. Unreliable-sequenced; receiver dedups against
  // confirmed frames.
  void sendInputBatch(uint8_t generation, uint32_t baseFrame, uint8_t count, uint8_t localDelta,
                      uint8_t const* inputs);
  void sendChecksum(uint8_t generation, uint32_t frame, uint32_t checksum);
  void sendHandshake(uint32_t seed, uint32_t settingsHash);
  void sendPlayerInfo(const PlayerInfo& info);
  void sendMatchSettings(const MatchSettingsData& data);
  void sendMapData(const void* data, size_t len);
  void sendPause();
  void sendResume();
  void sendRematchReady(bool ready);
  void sendRematchLevel(bool randomLevel, const std::string& levelFile);
  void sendEndMatch();
  void sendPeerLeft();
  void sendTcInfo(uint32_t hash, const std::string& name);
  void sendTcResponse(bool needData);
  void sendTcData(const void* data, size_t len);

  State state() const { return state_; }
  uint16_t listeningPort() const;

  // Access the ENet host (for STUN-via-host integration)
  _ENetHost* enetHost() const { return enetHost_; }

  // Callbacks
  std::function<void(uint32_t frame, uint8_t input)> onRemoteInput;
  // Rollback batched input arrival. `remoteLocalFrame` = sender's
  // simFrame at send time (= baseFrame + localDelta) — used for
  // frame-advantage tracking. `inputs` is valid only for the duration
  // of the callback; copy if you need to keep it.
  std::function<void(uint8_t generation, uint32_t baseFrame, uint8_t count, uint8_t const* inputs,
                     uint32_t remoteLocalFrame)>
      onRemoteInputBatch;
  std::function<void(uint32_t seed, uint32_t settingsHash)> onHandshake;
  std::function<void(uint8_t generation, uint32_t frame, uint32_t checksum)> onChecksum;
  std::function<void(const PlayerInfo& info)> onPlayerInfo;
  std::function<void(const MatchSettingsData& data)> onMatchSettings;
  std::function<void(const void* data, size_t len)> onMapData;
  std::function<void()> onPause;
  std::function<void()> onResume;
  std::function<void(bool ready)> onRematchReady;
  std::function<void(bool randomLevel, std::string levelFile)> onRematchLevel;
  std::function<void()> onEndMatch;
  std::function<void()> onPeerLeft;
  std::function<void(uint32_t hash, std::string name)> onTcInfo;
  std::function<void(bool needData)> onTcResponse;
  std::function<void(const void* data, size_t len)> onTcData;
  std::function<void()> onConnected;
  std::function<void()> onDisconnected;
  // Called for each non-ENet packet intercepted (STUN, etc.)
  // Return true if consumed.
  std::function<bool(const uint8_t* data, size_t len)> onInterceptedPacket;

 private:
  void sendPacket(const void* data, size_t len);
  bool createHost(uint16_t port);
  void setupIntercept();

  static int interceptCallback(_ENetHost* host, void* event);

  _ENetHost* enetHost_;
  _ENetPeer* peer_;
  State state_;
  std::unique_ptr<IceBridge> iceBridge_;
  std::unique_ptr<IceAgent> iceAgent_;
};
