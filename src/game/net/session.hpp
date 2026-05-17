#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "../controller/networkController.hpp"
#include "transport.hpp"

// Wires NetworkController and NetTransport together.
// Manages the connection lifecycle: connect → handshake → play → disconnect.
struct NetSession {
  enum Role { Host, Client };
  enum SessionState {
    Idle,            // Not started
    WaitingForPeer,  // Listening (host) or connecting (client)
    Handshaking,     // Connected, exchanging handshakes
    Playing,         // Game running
    Disconnected,    // Peer left
    Failed,          // Connection failed
  };

  NetSession(std::shared_ptr<Common> common, std::shared_ptr<Settings> settings);
  ~NetSession();

  // Start as host. Listens on the given port.
  bool hostGame(uint16_t port);

  // Start as client. Connects to host at address:port.
  bool joinGame(const std::string& address, uint16_t port);

  // Call once per frame from the game loop.
  // Polls network, manages state transitions.
  void update();

  // Disconnect and clean up.
  void disconnect();

  SessionState sessionState() const { return sessionState_; }

  // The controller. Valid after construction but game doesn't start
  // until Playing state. Returns null if not yet created.
  NetworkController* controller() { return controllerPtr_; }

  // Release ownership of the controller (for handing to Gfx).
  // The session keeps a raw pointer for injecting remote inputs.
  std::unique_ptr<NetworkController> releaseController();

  // Access the transport (for testing)
  NetTransport& transport() { return transport_; }

 private:
  void onConnected();
  void onDisconnected();
  void onHandshake(uint32_t seed, uint32_t settingsHash);
  void onPlayerInfo(const NetTransport::PlayerInfo& info);
  void onMatchSettings(const NetTransport::MatchSettingsData& data);
  void onMapData(const void* data, size_t len);
  void onRemoteInput(uint32_t frame, uint8_t input);
  void wireCallbacks();
  void tryStartGame();
  void generateAndSendMap();
  uint32_t computeSettingsHash() const;

  Role role_;
  SessionState sessionState_;
  NetTransport transport_;
  std::unique_ptr<NetworkController> controller_;
  std::shared_ptr<Common> common_;
  std::shared_ptr<Settings> settings_;

  NetworkController* controllerPtr_;  // non-owning, survives releaseController()

  uint32_t gameSeed_;
  uint32_t localSettingsHash_;
  bool handshakeReceived_;
  bool handshakeSent_;
  bool playerInfoReceived_;
  bool matchSettingsReceived_;  // client only; host always has settings
  bool mapDataReceived_;        // client only; host generates locally
  NetTransport::PlayerInfo remotePlayerInfo_;

  // Stored compressed map data (client receives from host)
  std::vector<uint8_t> receivedMapData_;

  static constexpr uint16_t DEFAULT_PORT = 19532;
};
