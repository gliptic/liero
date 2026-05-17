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
    PacketPlayerInfo = 4, // Player info: [type(1) | weapons(5*4) | color(4) | rgb(3*4)]
    PacketMatchSettings = 5, // Host-authoritative: [type(1) | settings blob]
    PacketMapData = 6,    // Compressed map: [type(1) | width(2) | height(2) | compressedData...]
    PacketPause = 7,      // Pause request: [type(1)]
    PacketResume = 8,     // Resume request: [type(1)]
    PacketRematchReady = 9,  // Ready toggle: [type(1) | ready(1)]
    PacketRematchLevel = 10, // Level selection: [type(1) | randomLevel(1) | levelFile(N)]
    PacketEndMatch = 11,     // End match request: [type(1)]
  };

  // Player info exchanged between peers (weapons + cosmetics)
  struct PlayerInfo {
    uint32_t weapons[5];
    int32_t color;
    int32_t rgb[3];
    char name[24];  // Fixed-size, null-terminated
  };

  // Match settings sent from host to client
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

  // Disconnect and release resources (returns to initial state).
  void disconnect();

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

  // Send local player's info (weapons + color)
  void sendPlayerInfo(const PlayerInfo& info);

  // Send match settings (host only)
  void sendMatchSettings(const MatchSettingsData& data);

  // Send compressed map data (host only)
  void sendMapData(const void* data, size_t len);

  // Send pause/resume notifications
  void sendPause();
  void sendResume();

  // Send rematch ready state
  void sendRematchReady(bool ready);

  // Send rematch level selection (host only)
  void sendRematchLevel(bool randomLevel, const std::string& levelFile);

  // Send end-match request (either player can end the match early)
  void sendEndMatch();

  State state() const { return state_; }

  // Returns the port the host is listening on (useful when binding to port 0).
  uint16_t listeningPort() const;

  // Callbacks set by the controller
  std::function<void(uint32_t frame, uint8_t input)> onRemoteInput;
  std::function<void(uint32_t seed, uint32_t settingsHash)> onHandshake;
  std::function<void(uint32_t frame, uint32_t checksum)> onChecksum;
  std::function<void(const PlayerInfo& info)> onPlayerInfo;
  std::function<void(const MatchSettingsData& data)> onMatchSettings;
  std::function<void(const void* data, size_t len)> onMapData;
  std::function<void()> onPause;
  std::function<void()> onResume;
  std::function<void(bool ready)> onRematchReady;
  std::function<void(bool randomLevel, std::string levelFile)> onRematchLevel;
  std::function<void()> onEndMatch;
  std::function<void()> onConnected;
  std::function<void()> onDisconnected;

 private:
  void sendPacket(const void* data, size_t len);

  _ENetHost* enetHost_;
  _ENetPeer* peer_;
  State state_;
};
