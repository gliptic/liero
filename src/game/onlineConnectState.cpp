#include "onlineConnectState.hpp"

#include "gfx.hpp"
#include "text.hpp"
#include "keys.hpp"
#include "common.hpp"
#include "netConnectState.hpp"
#include "net/netutil.hpp"

#include <memory>
#include <string>
#include <cstdio>

using netutil::nowMs;

OnlineConnectState::OnlineConnectState(NetSession::Role role, std::string roomCode)
: role_(role)
, roomCode_(std::move(roomCode))
{
}

void OnlineConnectState::enter()
{
	fprintf(stderr, "[online] enter: role=%s roomCode='%s'\n",
	        role_ == NetSession::Host ? "Host" : "Client", roomCode_.c_str());

	statusLine1_ = (role_ == NetSession::Host)
		? "CREATING ONLINE ROOM..."
		: "JOINING ROOM " + roomCode_ + "...";
	statusLine2_ = "SETTING UP ICE...";

	iceAgent_ = std::make_unique<IceAgent>();
	iceBridge_ = std::make_unique<IceBridge>();

	// Wire IceAgent callbacks
	iceAgent_->onLocalCandidate = [this](const std::string& candidate) {
		if (signalingReady_) {
			signaling_.sendIceCandidate(candidate);
		} else {
			bufferedCandidates_.push_back(candidate);
		}
	};

	iceAgent_->onGatheringDone = [this]() {
		gatheringDone_ = true;
		if (signalingReady_) {
			signaling_.sendIceGatherDone();
		}
	};

	iceAgent_->onStateChange = [this](IceAgent::State state) {
		fprintf(stderr, "[online] ICE state: %d\n", (int)state);
		switch (state) {
			case IceAgent::State::Gathering:
				statusLine2_ = "GATHERING CANDIDATES...";
				break;
			case IceAgent::State::Connecting:
				statusLine2_ = "ICE CONNECTING...";
				break;
			case IceAgent::State::Connected:
				iceConnected_ = true;
				statusLine2_ = "CONNECTED! STARTING GAME...";
				break;
			case IceAgent::State::Failed:
				statusLine2_ = "CONNECTION FAILED";
				cancel_ = true;
				break;
			default:
				break;
		}
	};

	// Start ICE agent (begins gathering immediately)
	IceAgent::Config iceCfg;
	iceCfg.stunServer = "stun.l.google.com";
	iceCfg.stunPort = 19302;
	// TURN credentials will be set after signaling responds
	iceAgent_->start(iceCfg);

	// Start signaling immediately (don't wait for STUN — ICE handles it)
	startSignaling();
}

void OnlineConnectState::startSignaling()
{
	// Wire signaling callbacks
	signaling_.onRoomCreated = [this](const std::string& code) {
		fprintf(stderr, "[online] onRoomCreated: code=%s\n", code.c_str());
		roomCode_ = code;
		statusLine1_ = "ROOM CODE: " + code;
		statusLine2_ = "WAITING FOR PEER...";

		// Room created but peer hasn't joined yet — don't send ICE yet
	};

	signaling_.onPeerJoined = [this]() {
		fprintf(stderr, "[online] onPeerJoined\n");
		statusLine2_ = "PEER JOINED! EXCHANGING ICE...";

		// Now send our ICE credentials and buffered candidates
		signalingReady_ = true;
		signaling_.sendIceCredentials(iceAgent_->localUfrag(), iceAgent_->localPwd());
		sendBufferedCandidates();
	};

	signaling_.onJoinAcked = [this]() {
		fprintf(stderr, "[online] join acknowledged\n");
		statusLine2_ = "JOINED! EXCHANGING ICE...";

		// Send our ICE credentials and buffered candidates
		signalingReady_ = true;
		signaling_.sendIceCredentials(iceAgent_->localUfrag(), iceAgent_->localPwd());
		sendBufferedCandidates();
	};

	// ICE callbacks from signaling
	signaling_.onPeerCredentials = [this](const std::string& ufrag, const std::string& pwd) {
		fprintf(stderr, "[online] onPeerCredentials: ufrag=%s\n", ufrag.c_str());
		iceAgent_->setRemoteCredentials(ufrag, pwd);
	};

	signaling_.onPeerCandidate = [this](const std::string& candidate) {
		iceAgent_->addRemoteCandidate(candidate);
	};

	signaling_.onPeerGatherDone = [this]() {
		// Not calling iceAgent_.setRemoteGatheringDone() — it's optional in ICE.
		// Omitting it avoids a race where UDP reordering delivers gather-done
		// before late candidates (which libjuice would then reject).
		// libjuice still works correctly without it; it just uses its internal
		// timeout to conclude that no more candidates are coming.
	};

	signaling_.onError = [this](const std::string& msg) {
		fprintf(stderr, "[online] onError: %s\n", msg.c_str());
		statusLine2_ = "ERROR: " + msg;
		cancel_ = true;
	};

	signaling_.onRoomExpired = [this]() {
		fprintf(stderr, "[online] onRoomExpired\n");
		statusLine2_ = "ROOM EXPIRED";
		cancel_ = true;
	};

	// Connect to signaling server
	if (role_ == NetSession::Host) {
		if (!signaling_.createRoom(signalingServer_, signalingPort_)) {
			statusLine2_ = "FAILED TO REACH SIGNALING SERVER";
			cancel_ = true;
		}
	} else {
		if (!signaling_.joinRoom(signalingServer_, signalingPort_, roomCode_)) {
			statusLine2_ = "FAILED TO REACH SIGNALING SERVER";
			cancel_ = true;
		}
	}
}

void OnlineConnectState::sendBufferedCandidates()
{
	for (auto& c : bufferedCandidates_)
		signaling_.sendIceCandidate(c);
	bufferedCandidates_.clear();

	if (gatheringDone_)
		signaling_.sendIceGatherDone();
}

void OnlineConnectState::handleEvent(SDL_Event& ev)
{
	gfx->processEvent(ev);
}

bool OnlineConnectState::update()
{
	if (cancel_)
	{
		if (gfx->testSDLKeyOnce(SDL_SCANCODE_ESCAPE) || gfx->testSDLKeyOnce(SDL_SCANCODE_RETURN))
		{
			if (iceAgent_) iceAgent_->stop();
			signaling_.disconnect();
			return false;
		}
		return true;
	}

	if (gfx->testSDLKeyOnce(SDL_SCANCODE_ESCAPE))
	{
		if (iceAgent_) iceAgent_->stop();
		signaling_.disconnect();
		gfx->clearKeys();
		return false;
	}

	// Poll ICE agent (drains event queue, fires callbacks on main thread)
	if (iceAgent_) iceAgent_->poll();

	// Poll signaling
	signaling_.poll();

	// Keepalive
	if (signaling_.state() != SignalingClient::Idle &&
	    signaling_.state() != SignalingClient::Failed)
	{
		uint64_t now = nowMs();
		if (now - lastKeepaliveMs_ > 5000) {
			signaling_.sendKeepalive();
			lastKeepaliveMs_ = now;
		}
	}

	// Transition to game when ICE connects
	if (iceConnected_) {
		transitionToGame();
		iceConnected_ = false; // prevent re-entry
	}

	return true;
}

void OnlineConnectState::transitionToGame()
{
	fprintf(stderr, "[online] ICE connected — creating bridge and transitioning to game\n");

	// Disconnect signaling (no longer needed)
	signaling_.disconnect();

	// Create the loopback bridge
	auto bridgeFd = iceBridge_->create(*iceAgent_);
	if (bridgeFd == BRIDGE_INVALID) {
		fprintf(stderr, "[online] ERROR: failed to create ICE bridge\n");
		statusLine2_ = "BRIDGE CREATION FAILED";
		cancel_ = true;
		return;
	}

	uint16_t bport = iceBridge_->bridgePort();

	// Create a NetTransport using the bridge socket
	NetTransport transport;
	if (!transport.createHostOnBridgeSocket(bridgeFd)) {
		fprintf(stderr, "[online] ERROR: failed to create ENet host on bridge socket\n");
		statusLine2_ = "ENET BRIDGE SETUP FAILED";
		cancel_ = true;
		return;
	}

	// Transfer ICE ownership to the transport (keeps them alive after this state is destroyed)
	// Clear callbacks first — they capture `this` which will be destroyed
	iceAgent_->onStateChange = nullptr;
	iceAgent_->onLocalCandidate = nullptr;
	iceAgent_->onGatheringDone = nullptr;
	transport.attachIce(std::move(iceBridge_), std::move(iceAgent_));

	// Transition to NetConnectState with the bridge-backed transport.
	// For host: ENet listens on bridge socket, peer connects.
	// For client: ENet connects to bridge port (IPv6 localhost).
	if (role_ == NetSession::Host) {
		gfx->stateStack.scheduleReplaceTop(
			std::make_unique<NetConnectState>(NetSession::Host, std::move(transport)));
	} else {
		gfx->stateStack.scheduleReplaceTop(
			std::make_unique<NetConnectState>(NetSession::Client, std::move(transport),
			                                  "::1", bport));
	}
}

void OnlineConnectState::draw()
{
	Common& common = *gfx->common;
	Font& font = common.font;

	gfx->playRenderer.pal = common.exepal;
	fill(gfx->playRenderer.bmp, 0);

	int cx = 160;
	int cy = 60;

	int w1 = font.getDims(statusLine1_);
	font.drawText(gfx->playRenderer.bmp, statusLine1_, cx - w1 / 2, cy, 50);

	int w2 = font.getDims(statusLine2_);
	font.drawText(gfx->playRenderer.bmp, statusLine2_, cx - w2 / 2, cy + 14, 7);

	if (role_ == NetSession::Host && !roomCode_.empty()
	    && signaling_.state() == SignalingClient::Hosting)
	{
		std::string codeStr = "CODE: " + roomCode_;
		int wc = font.getDims(codeStr);
		font.drawText(gfx->playRenderer.bmp, codeStr, cx - wc / 2, cy + 30, 45);
	}

	std::string esc = "PRESS ESC TO CANCEL";
	int we = font.getDims(esc);
	font.drawText(gfx->playRenderer.bmp, esc, cx - we / 2, 170, 7);
}
