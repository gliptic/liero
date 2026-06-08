#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <enet.h>

// Candidate address for a peer (discovered via STUN or reported by signaling).
// Used by the legacy hole-punch path.
struct PeerCandidate {
  uint8_t type;  // 4 = IPv4, 6 = IPv6
  std::string ip;
  uint16_t port;
};

// Client for the openliero signaling server.
// Uses ENet raw UDP sockets — no WebSocket dependency.
class SignalingClient {
 public:
  enum State {
    kIdle,
    kCreating,
    kHosting,
    kJoining,
    kWaitingForPeer,
    kPunching,  // legacy
    kRelaying,  // legacy
    kIceExchanging,
    kFailed,
    kDone,
  };

  SignalingClient();
  ~SignalingClient();

  bool CreateRoom(const std::string& server_addr, uint16_t server_port);
  bool JoinRoom(const std::string& server_addr, uint16_t server_port, const std::string& room_code);

  // Legacy hole-punch methods (kept for transition)
  void ReportAddress(uint8_t addr_type, const std::string& ip, uint16_t port);
  void ReportPunchOk();
  void ReportPunchFail();

  // ICE signaling methods
  void SendIceCredentials(const std::string& ufrag, const std::string& pwd);
  void SendIceCandidate(const std::string& sdp_candidate);
  void SendIceGatherDone();

  void SendKeepalive();
  void Poll();
  void Disconnect();

  State State() const { return state_; }
  const std::string& RoomCode() const { return roomCode_; }
  const std::vector<PeerCandidate>& PeerCandidates() const { return peerCandidates_; }
  uint16_t RelayPort() const { return relayPort_; }
  const std::vector<uint8_t>& RelayToken() const { return relayToken_; }

  // TURN credentials received from the server
  const std::string& TurnUser() const { return turnUser_; }
  const std::string& TurnPassword() const { return turnPassword_; }

  // Legacy callbacks
  std::function<void(const std::string& code)> on_room_created;
  std::function<void()> on_peer_joined;
  std::function<void()> on_join_acked;
  std::function<void(const PeerCandidate&)> on_peer_addr;
  std::function<void()> on_start_punch;
  std::function<void(uint16_t relay_port)> on_use_relay;

  // ICE callbacks
  std::function<void(const std::string& ufrag, const std::string& pwd)> on_peer_credentials;
  std::function<void(const std::string& sdp_candidate)> on_peer_candidate;
  std::function<void()> on_peer_gather_done;

  // Common callbacks
  std::function<void(const std::string& msg)> on_error;
  std::function<void()> on_room_expired;

 private:
  bool Connect(const std::string& server_addr, uint16_t server_port);
  void Send(const void* data, size_t len);
  void HandleMessage(const uint8_t* data, size_t len);

  ENetSocket sock_{ENET_SOCKET_NULL};
  enum State state_ { kIdle };
  std::string roomCode_;
  std::string serverAddr_;
  uint16_t serverPort_{0};
  std::vector<PeerCandidate> peerCandidates_;
  uint16_t relayPort_{0};
  std::vector<uint8_t> relayToken_;
  std::string turnUser_;
  std::string turnPassword_;
  int pollErrCount_ = 0;

  uint64_t lastSendMs_ = 0;
  int retryCount_ = 0;
  static constexpr int kRetryIntervalMs = 2000;
  static constexpr int kMaxRetries = 5;

  ENetAddress resolvedAddr_ = {};
};
