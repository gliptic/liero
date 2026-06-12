#include "onlineConnectState.hpp"

#include "common.hpp"
#include "gfx.hpp"
#include "keys.hpp"
#include "net/netutil.hpp"
#include "netConnectState.hpp"
#include "text.hpp"

#include <cstdio>
#include <memory>
#include <string>

using netutil::NowMs;

OnlineConnectState::OnlineConnectState(NetSession::Role role, std::string room_code)
    : role_(role), roomCode_(std::move(room_code)) {}

void OnlineConnectState::Enter() {
  std::fprintf(stderr, "[online] enter: role=%s roomCode='%s'\n",
               role_ == NetSession::kHost ? "Host" : "Client", roomCode_.c_str());

  statusLine1_ = (role_ == NetSession::kHost) ? "CREATING ONLINE ROOM..."
                                              : "JOINING ROOM " + roomCode_ + "...";
  statusLine2_ = "SETTING UP ICE...";

  iceAgent_ = std::make_unique<IceAgent>();
  iceBridge_ = std::make_unique<IceBridge>();

  // Wire IceAgent callbacks
  iceAgent_->on_local_candidate = [this](const std::string& candidate) {
    if (signalingReady_) {
      signaling_.SendIceCandidate(candidate);
    } else {
      bufferedCandidates_.push_back(candidate);
    }
  };

  iceAgent_->on_gathering_done = [this]() {
    gatheringDone_ = true;
    if (signalingReady_) {
      signaling_.SendIceGatherDone();
    }
  };

  iceAgent_->on_state_change = [this](IceAgent::State state) {
    std::fprintf(stderr, "[online] ICE state: %d\n", static_cast<int>(state));
    switch (state) {
      case IceAgent::State::kGathering:
        statusLine2_ = "GATHERING CANDIDATES...";
        break;
      case IceAgent::State::kConnecting:
        statusLine2_ = "ICE CONNECTING...";
        break;
      case IceAgent::State::kConnected:
        iceConnected_ = true;
        statusLine2_ = "CONNECTED! STARTING GAME...";
        break;
      case IceAgent::State::kFailed:
        statusLine2_ = "CONNECTION FAILED";
        cancel_ = true;
        break;
      default:
        break;
    }
  };

  // Start ICE agent (begins gathering immediately)
  IceAgent::Config ice_cfg;
  ice_cfg.stun_server = "stun.l.google.com";
  ice_cfg.stun_port = 19302;
  // TURN credentials will be set after signaling responds
  iceAgent_->Start(ice_cfg);

  // Start signaling immediately (don't wait for STUN — ICE handles it)
  StartSignaling();
}

void OnlineConnectState::StartSignaling() {
  // Wire signaling callbacks
  signaling_.on_room_created = [this](const std::string& code) {
    std::fprintf(stderr, "[online] onRoomCreated: code=%s\n", code.c_str());
    roomCode_ = code;
    statusLine1_ = "ROOM CODE: " + code;
    statusLine2_ = "WAITING FOR PEER...";

    // Room created but peer hasn't joined yet — don't send ICE yet
  };

  signaling_.on_peer_joined = [this]() {
    std::fprintf(stderr, "[online] onPeerJoined\n");
    statusLine2_ = "PEER JOINED! EXCHANGING ICE...";

    // Now send our ICE credentials and buffered candidates
    signalingReady_ = true;
    signaling_.SendIceCredentials(iceAgent_->LocalUfrag(), iceAgent_->LocalPwd());
    SendBufferedCandidates();
  };

  signaling_.on_join_acked = [this]() {
    std::fprintf(stderr, "[online] join acknowledged\n");
    statusLine2_ = "JOINED! EXCHANGING ICE...";

    // Send our ICE credentials and buffered candidates
    signalingReady_ = true;
    signaling_.SendIceCredentials(iceAgent_->LocalUfrag(), iceAgent_->LocalPwd());
    SendBufferedCandidates();
  };

  // ICE callbacks from signaling
  signaling_.on_peer_credentials = [this](const std::string& ufrag, const std::string& pwd) {
    std::fprintf(stderr, "[online] onPeerCredentials: ufrag=%s\n", ufrag.c_str());
    iceAgent_->SetRemoteCredentials(ufrag, pwd);
  };

  signaling_.on_peer_candidate = [this](const std::string& candidate) {
    iceAgent_->AddRemoteCandidate(candidate);
  };

  signaling_.on_peer_gather_done = [this]() {
    // Not calling iceAgent_.setRemoteGatheringDone() — it's optional in ICE.
    // Omitting it avoids a race where UDP reordering delivers gather-done
    // before late candidates (which libjuice would then reject).
    // libjuice still works correctly without it; it just uses its internal
    // timeout to conclude that no more candidates are coming.
  };

  signaling_.on_error = [this](const std::string& msg) {
    std::fprintf(stderr, "[online] onError: %s\n", msg.c_str());
    statusLine2_ = "ERROR: " + msg;
    cancel_ = true;
  };

  signaling_.on_room_expired = [this]() {
    std::fprintf(stderr, "[online] onRoomExpired\n");
    statusLine2_ = "ROOM EXPIRED";
    cancel_ = true;
  };

  // Connect to signaling server
  if (role_ == NetSession::kHost) {
    if (!signaling_.CreateRoom(signaling_server_, signalingPort_)) {
      statusLine2_ = "FAILED TO REACH SIGNALING SERVER";
      cancel_ = true;
    }
  } else {
    if (!signaling_.JoinRoom(signaling_server_, signalingPort_, roomCode_)) {
      statusLine2_ = "FAILED TO REACH SIGNALING SERVER";
      cancel_ = true;
    }
  }
}

void OnlineConnectState::SendBufferedCandidates() {
  for (auto& c : bufferedCandidates_) {
    signaling_.SendIceCandidate(c);
  }
  bufferedCandidates_.clear();

  if (gatheringDone_) {
    signaling_.SendIceGatherDone();
  }
}

void OnlineConnectState::HandleEvent(SDL_Event& ev) { gfx->ProcessEvent(ev); }

bool OnlineConnectState::Update() {
  if (cancel_) {
    if (gfx->TestSdlKeyOnce(SDL_SCANCODE_ESCAPE) || gfx->TestSdlKeyOnce(SDL_SCANCODE_RETURN)) {
      if (iceAgent_) {
        iceAgent_->Stop();
      }
      signaling_.Disconnect();
      return false;
    }
    return true;
  }

  if (gfx->TestSdlKeyOnce(SDL_SCANCODE_ESCAPE)) {
    if (iceAgent_) {
      iceAgent_->Stop();
    }
    signaling_.Disconnect();
    gfx->ClearKeys();
    return false;
  }

  // Poll ICE agent (drains event queue, fires callbacks on main thread)
  if (iceAgent_) {
    iceAgent_->Poll();
  }

  // Poll signaling
  signaling_.Poll();

  // Keepalive
  if (signaling_.State() != SignalingClient::kIdle &&
      signaling_.State() != SignalingClient::kFailed) {
    uint64_t const kNow = NowMs();
    if (kNow - lastKeepaliveMs_ > 5000) {
      signaling_.SendKeepalive();
      lastKeepaliveMs_ = kNow;
    }
  }

  // Transition to game when ICE connects
  if (iceConnected_) {
    TransitionToGame();
    iceConnected_ = false;  // prevent re-entry
  }

  return true;
}

void OnlineConnectState::TransitionToGame() {
  std::fprintf(stderr, "[online] ICE connected — creating bridge and transitioning to game\n");

  // Disconnect signaling (no longer needed)
  signaling_.Disconnect();

  // Create the loopback bridge
  auto bridge_fd = iceBridge_->Create(*iceAgent_);
  if (bridge_fd == kBridgeInvalid) {
    std::fprintf(stderr, "[online] ERROR: failed to create ICE bridge\n");
    statusLine2_ = "BRIDGE CREATION FAILED";
    cancel_ = true;
    return;
  }

  uint16_t const kBport = iceBridge_->BridgePort();

  // Create a NetTransport using the bridge socket
  NetTransport transport;
  if (!transport.CreateHostOnBridgeSocket(bridge_fd)) {
    std::fprintf(stderr, "[online] ERROR: failed to create ENet host on bridge socket\n");
    statusLine2_ = "ENET BRIDGE SETUP FAILED";
    cancel_ = true;
    return;
  }

  // Transfer ICE ownership to the transport (keeps them alive after this state is destroyed)
  // Clear callbacks first — they capture `this` which will be destroyed
  iceAgent_->on_state_change = nullptr;
  iceAgent_->on_local_candidate = nullptr;
  iceAgent_->on_gathering_done = nullptr;
  transport.AttachIce(std::move(iceBridge_), std::move(iceAgent_));

  // Transition to NetConnectState with the bridge-backed transport.
  // For host: ENet listens on bridge socket, peer connects.
  // For client: ENet connects to bridge port (IPv6 localhost).
  if (role_ == NetSession::kHost) {
    gfx->state_stack.ScheduleReplaceTop(
        std::make_unique<NetConnectState>(NetSession::kHost, std::move(transport)));
  } else {
    gfx->state_stack.ScheduleReplaceTop(std::make_unique<NetConnectState>(
        NetSession::kClient, std::move(transport), "::1", kBport));
  }
}

void OnlineConnectState::Draw() {
  Common& common = *gfx->common;
  Font& font = common.font;

  gfx->play_renderer.pal =
      gfx->play_renderer.mode == ColorMode::kModern ? common.modernpal : common.exepal;
  Fill(gfx->play_renderer.bmp, 0);

  int const kCx = 160;
  int const kCy = 60;

  int const kW1 = font.GetDims(statusLine1_);
  font.DrawString(gfx->play_renderer.bmp, statusLine1_, kCx - kW1 / 2, kCy, 50);

  int const kW2 = font.GetDims(statusLine2_);
  font.DrawString(gfx->play_renderer.bmp, statusLine2_, kCx - kW2 / 2, kCy + 14, 7);

  if (role_ == NetSession::kHost && !roomCode_.empty() &&
      signaling_.State() == SignalingClient::kHosting) {
    std::string const kCodeStr = "CODE: " + roomCode_;
    int const kWc = font.GetDims(kCodeStr);
    font.DrawString(gfx->play_renderer.bmp, kCodeStr, kCx - kWc / 2, kCy + 30, 45);
  }

  std::string const kEsc = "PRESS ESC TO CANCEL";
  int const kWe = font.GetDims(kEsc);
  font.DrawString(gfx->play_renderer.bmp, kEsc, kCx - kWe / 2, 170, 7);
}
