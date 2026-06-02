#pragma once

#include "net/localaddr.hpp"
#include "net/session.hpp"
#include "net/stun.hpp"
#include "net/transport.hpp"
#include "state.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Connection state — shows status while waiting for a peer to connect.
// On success, transfers the controller to Gfx and replaces itself with GamePlayState.
struct NetConnectState : AppState {
  NetConnectState(NetSession::Role role, std::string address, uint16_t port);
  // Direct connection with existing transport (after ICE)
  NetConnectState(NetSession::Role role, NetTransport&& transport, std::string peer_addr = "",
                  uint16_t peer_port = 0);

  void Enter() override;
  void HandleEvent(SDL_Event& ev) override;
  bool Update() override;
  void Draw() override;

 private:
  NetSession::Role role_;
  std::string address_;
  uint16_t port_;
  bool hasTransport_ = false;
  bool cancel_ = false;
  std::vector<LocalAddress> localAddresses_;
  std::unique_ptr<StunQuery> stunQuery_;
  StunResult externalIPs_;
  NetTransport existingTransport_;
};
