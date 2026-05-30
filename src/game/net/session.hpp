#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "../rollback/buffer.hpp"

#include "../controller/rollbackController.hpp"
#include "../filesystem.hpp"
#include "memoryFs.hpp"
#include "transport.hpp"

// Wires RollbackController and NetTransport together. Manages the
// connection lifecycle: connect → handshake → play → disconnect.
struct NetSession {
  enum Role { Host, Client };
  enum SessionState {
    Idle,            // Not started
    WaitingForPeer,  // Listening (host) or connecting (client)
    Handshaking,     // Connected, exchanging handshakes
    Playing,         // Game running
    Rematch,         // Post-game rematch screen
    Disconnected,    // Peer left
    Failed,          // Connection failed
  };

  NetSession(std::shared_ptr<Common> common, std::shared_ptr<Settings> settings,
             FsNode tcRoot);
  ~NetSession();

  // Start as host. Listens on the given port.
  bool hostGame(uint16_t port);

  // Start as client. Connects to host at address:port.
  bool joinGame(const std::string& address, uint16_t port);

  // Start with an existing transport (already connected or listening).
  // Used after ICE succeeds to hand the bridge-backed transport to the session.
  // For host: transport is already listening, peer will connect.
  // For client: initiates ENet connect to peerAddr:peerPort through existing host.
  bool hostWithTransport(NetTransport&& transport);
  bool connectWithTransport(NetTransport&& transport, const std::string& peerAddr, uint16_t peerPort);

  // Call once per frame from the game loop.
  // Polls network, manages state transitions.
  void update();

  // Disconnect and clean up.
  void disconnect();

  SessionState sessionState() const { return sessionState_; }

  RollbackController* rollbackController() { return rollbackPtr_; }

  // Release ownership of the controller (for handing to Gfx). The
  // session keeps a raw pointer for injecting remote inputs.
  std::unique_ptr<Controller> releaseController();

  // Send pause/resume to remote peer
  void sendPause();
  void sendResume();

  // Transition from Playing to Rematch state (keeps connection alive)
  void enterRematch();

  // Toggle local player's ready state and notify peer
  void toggleReady();

  // Change the level selection (host only) and notify peer
  void setRematchLevel(bool randomLevel, const std::string& levelFile);

  // Start the rematch game (called when both players are ready)
  void startRematch();

  // Rematch state accessors
  bool localReady() const { return localReady_; }
  bool remoteReady() const { return remoteReady_; }
  bool isHost() const { return role_ == Host; }

  // Desync detection
  bool desyncDetected() const { return desyncDetected_; }
  uint32_t desyncFrame() const { return desyncFrame_; }

  // TC sync: called when client needs to reload Common with new TC data.
  // The callback receives the new Common. Caller must update gfx.common.
  std::function<void(std::shared_ptr<Common>)> onTcReloaded;

  // Access the transport (for testing)
  NetTransport& transport() { return transport_; }

 private:
  void onConnected();
  void onDisconnected();
  void onHandshake(uint32_t seed, uint32_t settingsHash);
  void onPlayerInfo(const NetTransport::PlayerInfo& info);
  void onMatchSettings(const NetTransport::MatchSettingsData& data);
  void onMapData(const void* data, size_t len);
  void onPause();
  void onResume();
  void onRemoteEndMatch();
  void onRemotePeerLeft();
  void onRematchReady(bool ready);
  void onRematchLevel(bool randomLevel, std::string levelFile);
  void onRemoteInputBatch(uint8_t generation, uint32_t baseFrame,
                          uint8_t count, uint8_t const* inputs,
                          uint32_t remoteLocalFrame);
  void onTcInfo(uint32_t hash, std::string name);
  void onTcResponse(bool needData);
  void onTcData(const void* data, size_t len);
  void wireCallbacks();
  void tryStartGame();
  void startRematchClient();
  // Shared body of tryStartGame / startRematch / startRematchClient:
  // create controller, apply remote player info, seed RNG, prepare or
  // load the level, wire callbacks, pre-fill remote input, and enter
  // Playing. The host-rematch handshake send is not included here; the
  // caller does that before invoking.
  void beginPlaying(int localIdx, bool isRematch);
  void applyRemotePlayerInfo(int remoteIdx);
  void prefillRemoteInput();
  void generateAndSendMap();
  uint32_t computeSettingsHash() const;

  // Build the rollback controller and wire its transport callbacks.
  // Shared by all game-start paths.
  void createController(int localIdx);
  void wireActiveController();

  // Send a PlayerInfo packet derived from wormSettings[NetworkPlayerIdx].
  // Called on initial connect and again on rematch (where the prior
  // round's WeaponSelection has mutated weapons[] in place via the
  // shared shared_ptr).
  void sendLocalPlayerInfo();

  Game& activeGame();

  Role role_;
  SessionState sessionState_;
  NetTransport transport_;
  std::unique_ptr<RollbackController> rollback_;
  std::shared_ptr<Common> common_;
  std::shared_ptr<Settings> settings_;

  RollbackController* rollbackPtr_;    // non-owning

  uint32_t gameSeed_;
  uint32_t localSettingsHash_;
  bool handshakeReceived_;
  bool handshakeSent_;
  bool playerInfoReceived_;
  bool matchSettingsReceived_;  // client only; host always has settings
  bool mapDataReceived_;        // client only; host generates locally
  NetTransport::PlayerInfo remotePlayerInfo_;

  // Rematch state
  bool localReady_;
  bool remoteReady_;

  // Stored compressed map data (client receives from host)
  std::vector<uint8_t> receivedMapData_;

  // TC sync state
  FsNode tcRoot_;           // Root directory of the local TC
  uint32_t localTcHash_;    // Hash of local TC contents
  bool tcResolved_;         // True when TC exchange is complete
  std::shared_ptr<MemoryFs> tcMemFs_;  // Keeps received TC data alive in memory
  std::string originalTcName_;         // Client's original TC name (restored on disconnect)
  std::shared_ptr<Common> originalCommon_;  // Client's original Common (restored on disconnect)

  // Desync detection
  bool desyncDetected_;
  uint32_t desyncFrame_;
  void onChecksum(uint8_t generation, uint32_t frame, uint32_t checksum);
  void onLocalChecksum(uint32_t frame, uint32_t checksum);

  // Ring buffer of local checksums for frame-accurate comparison
  static constexpr size_t CHECKSUM_BUFFER_SIZE = 128;
  struct FrameChecksum { uint32_t frame; uint32_t checksum; bool valid; };
  FrameChecksum checksumBuffer_[CHECKSUM_BUFFER_SIZE] = {};
  // Pending remote checksums waiting for local frame to catch up
  struct PendingRemote { uint32_t frame; uint32_t checksum; };
  PendingRemote pendingRemoteChecksums_[CHECKSUM_BUFFER_SIZE] = {};
  size_t pendingRemoteCount_ = 0;

  // Held until beginPlaying wires up rollbackPtr_; otherwise the
  // host's first sends arrive during the asymmetric handshake window
  // and get dropped, and the K-wide window slides past those frames
  // before they can be re-sent.
  struct PendingInputBatch {
    uint8_t generation;
    uint32_t baseFrame;
    uint8_t count;
    std::array<uint8_t, rollback::kMaxRollback + 1> inputs;
    uint32_t remoteLocalFrame;
  };
  static constexpr size_t MAX_PRE_PLAYING_BATCHES = 256;
  std::vector<PendingInputBatch> prePlayingInputBatches_;

  static constexpr uint16_t DEFAULT_PORT = 19532;
};
