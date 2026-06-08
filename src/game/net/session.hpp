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
  enum Role { kHost, kClient };
  enum SessionState {
    kIdle,            // Not started
    kWaitingForPeer,  // Listening (host) or connecting (client)
    kHandshaking,     // Connected, exchanging handshakes
    kPlaying,         // Game running
    kRematch,         // Post-game rematch screen
    kDisconnected,    // Peer left
    kFailed,          // Connection failed
  };

  NetSession(std::shared_ptr<Common> common, std::shared_ptr<Settings> settings, FsNode tc_root);
  ~NetSession();

  // Start as host. Listens on the given port.
  bool HostGame(uint16_t port);

  // Start as client. Connects to host at address:port.
  bool JoinGame(const std::string& address, uint16_t port);

  // Start with an existing transport (already connected or listening).
  // Used after ICE succeeds to hand the bridge-backed transport to the session.
  // For host: transport is already listening, peer will connect.
  // For client: initiates ENet connect to peerAddr:peerPort through existing host.
  bool HostWithTransport(NetTransport&& transport);
  bool ConnectWithTransport(NetTransport&& transport, const std::string& peer_addr,
                            uint16_t peer_port);

  // Call once per frame from the game loop.
  // Polls network, manages state transitions.
  void Update();

  // Disconnect and clean up.
  void Disconnect();

  SessionState State() const { return sessionState_; }

  RollbackController* Rollback() { return rollbackPtr_; }

  // Release ownership of the controller (for handing to Gfx). The
  // session keeps a raw pointer for injecting remote inputs.
  std::unique_ptr<Controller> ReleaseController();

  // Send pause/resume to remote peer
  void SendPause();
  void SendResume();

  // Transition from Playing to Rematch state (keeps connection alive)
  void EnterRematch();

  // Toggle local player's ready state and notify peer
  void ToggleReady();

  // Change the level selection (host only) and notify peer
  void SetRematchLevel(bool random_level, const std::string& level_file);

  // Start the rematch game (called when both players are ready)
  void StartRematch();

  // Rematch state accessors
  bool LocalReady() const { return localReady_; }
  bool RemoteReady() const { return remoteReady_; }
  bool IsHost() const { return role_ == kHost; }

  // Desync detection
  bool DesyncDetected() const { return desyncDetected_; }
  uint32_t DesyncFrame() const { return desyncFrame_; }

  // TC sync: called when client needs to reload Common with new TC data.
  // The callback receives the new Common. Caller must update gfx.common.
  std::function<void(std::shared_ptr<Common>)> on_tc_reloaded;

  // Access the transport (for testing)
  NetTransport& Transport() { return transport_; }

 private:
  void OnConnected();
  void OnDisconnected();
  void OnHandshake(uint32_t seed, uint32_t settings_hash);
  void OnPlayerInfo(const NetTransport::PlayerInfo& info);
  void OnMatchSettings(const NetTransport::MatchSettingsData& data);
  void OnMapData(const void* data, size_t len);
  void OnPause();
  void OnResume();
  void OnRemoteEndMatch();
  void OnRemotePeerLeft();
  void OnRematchReady(bool ready);
  void OnRematchLevel(bool random_level, std::string level_file);
  void OnRemoteInputBatch(uint8_t generation, uint32_t base_frame, uint8_t count,
                          uint8_t const* inputs, uint32_t remote_local_frame);
  void OnTcInfo(uint32_t hash, const std::string& name);
  void OnTcResponse(bool need_data);
  void OnTcData(const void* data, size_t len);
  void WireCallbacks();
  void TryStartGame();
  void StartRematchClient();
  // Shared body of tryStartGame / startRematch / startRematchClient:
  // create controller, apply remote player info, seed RNG, prepare or
  // load the level, wire callbacks, pre-fill remote input, and enter
  // Playing. The host-rematch handshake send is not included here; the
  // caller does that before invoking.
  void BeginPlaying(int local_idx, bool is_rematch);
  void ApplyRemotePlayerInfo(int remote_idx);
  void PrefillRemoteInput();
  void GenerateAndSendMap();
  uint32_t ComputeSettingsHash() const;

  // Build the rollback controller and wire its transport callbacks.
  // Shared by all game-start paths.
  void CreateController(int local_idx);
  void WireActiveController();

  // Send a PlayerInfo packet derived from wormSettings[NetworkPlayerIdx].
  // Called on initial connect and again on rematch (where the prior
  // round's WeaponSelection has mutated weapons[] in place via the
  // shared shared_ptr).
  void SendLocalPlayerInfo();

  Game& ActiveGame();

  Role role_{kHost};
  enum SessionState sessionState_ { kIdle };
  NetTransport transport_;
  std::unique_ptr<RollbackController> rollback_;
  std::shared_ptr<Common> common_;
  std::shared_ptr<Settings> settings_;

  struct RollbackController* rollbackPtr_{nullptr};  // non-owning

  uint32_t gameSeed_{0};
  uint32_t localSettingsHash_{0};
  bool handshakeReceived_{false};
  bool handshakeSent_{false};
  bool playerInfoReceived_{false};
  bool matchSettingsReceived_{false};  // client only; host always has settings
  bool mapDataReceived_{false};        // client only; host generates locally
  NetTransport::PlayerInfo remotePlayerInfo_;

  // Rematch state
  bool localReady_{false};
  bool remoteReady_{false};

  // Stored compressed map data (client receives from host)
  std::vector<uint8_t> receivedMapData_;

  // TC sync state
  FsNode tcRoot_;                           // Root directory of the local TC
  uint32_t localTcHash_{0};                 // Hash of local TC contents
  bool tcResolved_{false};                  // True when TC exchange is complete
  std::shared_ptr<MemoryFs> tcMemFs_;       // Keeps received TC data alive in memory
  std::string originalTcName_;              // Client's original TC name (restored on disconnect)
  std::shared_ptr<Common> originalCommon_;  // Client's original Common (restored on disconnect)

  // Desync detection
  bool desyncDetected_{false};
  uint32_t desyncFrame_{0};
  void OnChecksum(uint8_t generation, uint32_t frame, uint32_t remote_checksum);
  void OnLocalChecksum(uint32_t frame, uint32_t checksum);

  // Ring buffer of local checksums for frame-accurate comparison
  static constexpr size_t kChecksumBufferSize = 128;
  struct FrameChecksum {
    uint32_t frame;
    uint32_t checksum;
    bool valid;
  };
  FrameChecksum checksumBuffer_[kChecksumBufferSize] = {};
  // Pending remote checksums waiting for local frame to catch up
  struct PendingRemote {
    uint32_t frame;
    uint32_t checksum;
  };
  PendingRemote pendingRemoteChecksums_[kChecksumBufferSize] = {};
  size_t pendingRemoteCount_ = 0;

  // Held until beginPlaying wires up rollbackPtr_; otherwise the
  // host's first sends arrive during the asymmetric handshake window
  // and get dropped, and the K-wide window slides past those frames
  // before they can be re-sent.
  struct PendingInputBatch {
    uint8_t generation;
    uint32_t base_frame;
    uint8_t count;
    std::array<uint8_t, rollback::kMaxRollback + 1> inputs;
    uint32_t remote_local_frame;
  };
  static constexpr size_t kMaxPrePlayingBatches = 256;
  std::vector<PendingInputBatch> prePlayingInputBatches_;

  static constexpr uint16_t kDefaultPort = 19532;
};
