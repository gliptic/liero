#pragma once

#include "net/iceAgent.hpp"
#include "net/iceBridge.hpp"
#include "net/session.hpp"
#include "net/signaling.hpp"
#include "state.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Manages the "online" connection flow via signaling server + ICE (libjuice).
//
// Flow:
// 1. Create IceAgent with STUN + TURN config
// 2. Connect to signaling server (create/join room)
// 3. Exchange ICE credentials + candidates via signaling
// 4. libjuice performs ICE connectivity checks (all candidate pairs)
// 5. On success → create IceBridge, hand socket to NetConnectState
struct OnlineConnectState : AppState {
  OnlineConnectState(NetSession::Role role, std::string room_code = "");

  void Enter() override;
  void HandleEvent(SDL_Event& ev) override;
  bool Update() override;
  void Draw() override;

 private:
  void StartSignaling();
  void SendBufferedCandidates();
  void TransitionToGame();

  NetSession::Role role_;
  std::string roomCode_;

  std::string signaling_server_ = "liero-server.orbmit.org";
  uint16_t signalingPort_ = 19533;
  std::string turn_server_ = "liero-server.orbmit.org";
  uint16_t turnPort_ = 3478;

  std::unique_ptr<IceAgent> iceAgent_;
  std::unique_ptr<IceBridge> iceBridge_;
  SignalingClient signaling_;

  // Buffered candidates gathered before signaling is ready
  std::vector<std::string> bufferedCandidates_;
  bool gatheringDone_ = false;
  bool signalingReady_ = false;
  bool iceConnected_ = false;

  std::string statusLine1_;
  std::string statusLine2_;
  bool cancel_ = false;
  uint64_t lastKeepaliveMs_ = 0;
};
