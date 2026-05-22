#pragma once

#include "state.hpp"
#include "net/session.hpp"
#include "net/localaddr.hpp"
#include "net/stun.hpp"
#include "net/transport.hpp"

#include <string>
#include <vector>
#include <cstdint>
#include <memory>

// Connection state — shows status while waiting for a peer to connect.
// On success, transfers the controller to Gfx and replaces itself with GamePlayState.
struct NetConnectState : AppState
{
	NetConnectState(NetSession::Role role, std::string address, uint16_t port);
	// Direct connection with existing transport (after ICE)
	NetConnectState(NetSession::Role role, NetTransport&& transport,
	                std::string peerAddr = "", uint16_t peerPort = 0);

	void enter() override;
	void handleEvent(SDL_Event& ev) override;
	bool update() override;
	void draw() override;

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
