#pragma once

#include "state.hpp"
#include "net/session.hpp"

#include <string>
#include <cstdint>

// Connection state — shows status while waiting for a peer to connect.
// On success, transfers the controller to Gfx and replaces itself with GamePlayState.
struct NetConnectState : AppState
{
	NetConnectState(NetSession::Role role, std::string address, uint16_t port);

	void enter() override;
	void handleEvent(SDL_Event& ev) override;
	bool update() override;
	void draw() override;

private:
	NetSession::Role role_;
	std::string address_;
	uint16_t port_;
	bool cancel_ = false;
};
