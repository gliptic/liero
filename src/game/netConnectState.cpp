#include "netConnectState.hpp"

#include "common.hpp"
#include "controller/controller.hpp"
#include "gamePlayState.hpp"
#include "gfx.hpp"
#include "inputState.hpp"
#include "keys.hpp"
#include "mixer/player.hpp"
#include "net/localaddr.hpp"
#include "text.hpp"

#include <memory>
#include <string>

NetConnectState::NetConnectState(NetSession::Role role, std::string address, uint16_t port)
    : role_(role), address_(std::move(address)), port_(port) {}

NetConnectState::NetConnectState(NetSession::Role role, NetTransport&& transport,
                                 std::string peer_addr, uint16_t peer_port)
    : role_(role),
      address_(std::move(peer_addr)),
      port_(peer_port),
      hasTransport_(true),
      existingTransport_(std::move(transport)) {}

void NetConnectState::Enter() {
  FsNode tc_root = gfx->GetConfigNode() / "TC" / gfx->settings->tc;
  auto session = std::make_unique<NetSession>(gfx->common, gfx->settings, tc_root);

  // When client receives a new TC, update global gfx.common
  session->on_tc_reloaded = [](std::shared_ptr<Common> new_common) {
    ::gfx.common = new_common;
    ::gfx.play_renderer.LoadPalette(*new_common);
    if (auto* dp = dynamic_cast<DefaultSoundPlayer*>(::gfx.sound_player.get()))
      dp->SetCommon(*new_common);
  };

  bool ok = false;
  if (hasTransport_) {
    // Use the existing transport (from ICE bridge)
    if (role_ == NetSession::kHost)
      ok = session->HostWithTransport(std::move(existingTransport_));
    else
      ok = session->ConnectWithTransport(std::move(existingTransport_), address_, port_);
  } else if (role_ == NetSession::kHost) {
    ok = session->HostGame(port_);
    if (ok) {
      localAddresses_ = GetLocalAddresses();
      stunQuery_ = std::make_unique<StunQuery>();
      stunQuery_->Start(port_);
    }
  } else
    ok = session->JoinGame(address_, port_);

  if (!ok) {
    gfx->pending_error_message = "FAILED TO START NETWORK";
    cancel_ = true;
    gfx->net_session.reset();
    return;
  }

  gfx->net_session = std::move(session);
}

void NetConnectState::HandleEvent(SDL_Event& ev) { gfx->ProcessEvent(ev); }

bool NetConnectState::Update() {
  if (cancel_) {
    if (!gfx->pending_error_message.empty()) {
      std::string msg = std::move(gfx->pending_error_message);
      gfx->pending_error_message.clear();
      gfx->state_stack.ScheduleReplaceTop(
          std::make_unique<InfoBoxState>(std::move(msg), 160, 100, true));
    }
    return false;
  }

  if (gfx->TestSdlKeyOnce(SDL_SCANCODE_ESCAPE)) {
    gfx->net_session.reset();
    gfx->ClearKeys();
    return false;
  }

  if (!gfx->net_session) return false;

  gfx->net_session->Update();

  // Check for STUN result
  if (externalIPs_.ipv4.empty() && externalIPs_.ipv6.empty() && stunQuery_ && stunQuery_->Done())
    externalIPs_ = stunQuery_->Result();

  auto state = gfx->net_session->State();

  if (state == NetSession::kPlaying) {
    // Connection established — transfer controller and start game
    auto ctrl = gfx->net_session->ReleaseController();
    gfx->controller = std::unique_ptr<Controller>(ctrl.release());
    gfx->state_stack.ScheduleReplaceTop(std::make_unique<GamePlayState>());
    return true;
  }

  if (state == NetSession::kFailed) {
    gfx->net_session.reset();
    gfx->state_stack.ScheduleReplaceTop(
        std::make_unique<InfoBoxState>("CONNECTION FAILED", 160, 100, true));
    return true;
  }

  if (state == NetSession::kDisconnected) {
    gfx->net_session.reset();
    gfx->state_stack.ScheduleReplaceTop(
        std::make_unique<InfoBoxState>("PEER DISCONNECTED", 160, 100, true));
    return true;
  }

  return true;
}

void NetConnectState::Draw() {
  Common& common = *gfx->common;
  Font& font = common.font;

  gfx->play_renderer.pal = common.exepal;
  Fill(gfx->play_renderer.bmp, 0);

  std::string line1;
  if (role_ == NetSession::kHost)
    line1 = "HOSTING ON PORT " + std::to_string(port_);
  else
    line1 = "CONNECTING TO " + address_;

  std::string line2;
  if (gfx->net_session) {
    switch (gfx->net_session->State()) {
      case NetSession::kWaitingForPeer:
        line2 = "WAITING FOR PEER...";
        break;
      case NetSession::kHandshaking:
        line2 = "HANDSHAKING...";
        break;
      default:
        line2 = "CONNECTING...";
        break;
    }
  } else {
    line2 = "STARTING...";
  }

  std::string line3 = "PRESS ESC TO CANCEL";

  int cx = 160;
  int cy = 60;

  int w1 = font.GetDims(line1);
  font.DrawString(gfx->play_renderer.bmp, line1, cx - w1 / 2, cy, 50);

  int w2 = font.GetDims(line2);
  font.DrawString(gfx->play_renderer.bmp, line2, cx - w2 / 2, cy + 12, 7);

  // Show local addresses when hosting
  if (role_ == NetSession::kHost && !localAddresses_.empty()) {
    int addr_y = cy + 30;
    std::string hdr = "CONNECT USING:";
    int wh = font.GetDims(hdr);
    font.DrawString(gfx->play_renderer.bmp, hdr, cx - wh / 2, addr_y, 6);
    addr_y += 10;

    for (auto& addr : localAddresses_) {
      std::string display;
      if (addr.is_i_pv6)
        display = "[" + addr.ip + "]:" + std::to_string(port_);
      else
        display = addr.ip + ":" + std::to_string(port_);
      int wd = font.GetDims(display);
      font.DrawString(gfx->play_renderer.bmp, display, cx - wd / 2, addr_y, 7);
      addr_y += 10;
    }

    if (!externalIPs_.ipv4.empty()) {
      uint16_t ext_port = externalIPs_.ipv4_port ? externalIPs_.ipv4_port : port_;
      std::string d = externalIPs_.ipv4 + ":" + std::to_string(ext_port) + " (EXTERNAL)";
      int wd = font.GetDims(d);
      font.DrawString(gfx->play_renderer.bmp, d, cx - wd / 2, addr_y, 45);
      addr_y += 10;
    }
    if (!externalIPs_.ipv6.empty()) {
      uint16_t ext_port = externalIPs_.ipv6_port ? externalIPs_.ipv6_port : port_;
      std::string d = "[" + externalIPs_.ipv6 + "]:" + std::to_string(ext_port) + " (EXTERNAL)";
      int wd = font.GetDims(d);
      font.DrawString(gfx->play_renderer.bmp, d, cx - wd / 2, addr_y, 45);
      addr_y += 10;
    }

    int w3 = font.GetDims(line3);
    font.DrawString(gfx->play_renderer.bmp, line3, cx - w3 / 2, addr_y + 6, 6);
  } else {
    int w3 = font.GetDims(line3);
    font.DrawString(gfx->play_renderer.bmp, line3, cx - w3 / 2, cy + 30, 6);
  }
}
