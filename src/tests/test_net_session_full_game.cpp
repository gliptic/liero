// Full network game-loop integration test (Layer B).
//
// Stands two NetSessions up over real ENet TCP/UDP loopback, drives the
// production state machine (handshake → settings → mapdata → kPlaying →
// real weapon select → game phase) and plays an aggressive KillEmAll
// match all the way to a natural game-over (a worm's lives reach 0).
//
// This is the end-to-end complement to the Layer A controller-pair
// tests (test_net_full_game.cpp): it exercises the same game-over path
// but through the actual NetSession / NetTransport wiring rather than an
// in-memory transport. Game-over is detected via Process() returning
// false, which the controller returns only once the kStateGameEnded
// fade has fully drained (rollbackController.cpp) — the real
// end-of-match signal a live session uses.
//
// Single case, no injected jitter: loopback is effectively lossless, so
// this validates the integration path, not rollback robustness (covered
// by Layer A). Weapon select IS exercised here (the session does not
// skip it), unlike Layer A.

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

#include "controller/rollbackController.hpp"
#include "game.hpp"
#include "math.hpp"
#include "net/session.hpp"
#include "rand.hpp"
#include "worm.hpp"

namespace {

struct Env {
  std::shared_ptr<Common> common;
  std::shared_ptr<Settings> settings;
  FsNode tc_root;

  Env() {
    PrecomputeTables();
    common = std::make_shared<Common>();
    tc_root = FsNode("data") / "TC" / "openliero";
    common->load(tc_root);
    settings = std::make_shared<Settings>();
    // lives=1 + low spawn health → the first kill ends the match, so a
    // full game terminates in a bounded number of ticks under loopback.
    settings->lives = 1;
    settings->loading_time = 0;
    settings->load_change = true;
    settings->random_level = true;
    settings->game_mode = Settings::kGmKillEmAll;
    settings->input_delay = 1;
    for (auto& ws : settings->worm_settings) {
      if (ws) {
        ws->health = 5;
      }
    }
  }
};

template <typename Pred>
bool PollUntil(NetSession& a, NetSession& b, Pred pred, int max_ms = 5000) {
  auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(max_ms);
  while (!pred() && std::chrono::steady_clock::now() < deadline) {
    a.Update();
    b.Update();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return pred();
}

uint8_t CombatInput(Rand& rng, int peer_idx) {
  uint8_t input = rng() & 0x7f;
  if ((rng() % 10) < 6) {
    input |= (1 << Worm::kFire);
  }
  if ((rng() % 10) < 4) {
    input |= (1 << (peer_idx == 0 ? 1 : 0));  // move toward opponent
  }
  return input;
}

}  // namespace

TEST_CASE("Layer B: two NetSessions reach game-over over loopback", "[net_session_full_game]") {
  Env const kE;

  // Each peer needs its own deep copy of the per-worm settings so the
  // single-process weapon-select mutations on one side don't bleed into
  // the other (mirrors test_session_rollback.cpp).
  auto host_settings = std::make_shared<Settings>(*kE.settings);
  auto client_settings = std::make_shared<Settings>(*kE.settings);
  for (auto* s : {&host_settings, &client_settings}) {
    for (auto& ws : (*s)->worm_settings) {
      if (ws) {
        ws = std::make_shared<WormSettings>(*ws);
      }
    }
  }

  NetSession host(kE.common, host_settings, kE.tc_root);
  NetSession client(kE.common, client_settings, kE.tc_root);

  REQUIRE(host.HostGame(0));
  uint16_t const kPort = host.Transport().ListeningPort();
  REQUIRE(client.JoinGame("127.0.0.1", kPort));

  bool const kReady = PollUntil(host, client, [&]() {
    return host.State() == NetSession::kPlaying && client.State() == NetSession::kPlaying;
  });
  REQUIRE(kReady);

  auto* hc = host.Rollback();
  auto* cc = client.Rollback();
  REQUIRE(hc != nullptr);
  REQUIRE(cc != nullptr);

  hc->Focus();
  cc->Focus();

  constexpr uint8_t kBitDown = uint8_t{1} << Worm::kDown;
  constexpr uint8_t kBitFire = uint8_t{1} << Worm::kFire;

  bool host_transitioned = false;
  bool client_transitioned = false;

  auto run_tick = [&](uint8_t in_h, uint8_t in_c) {
    hc->SetLocalControlState(in_h);
    cc->SetLocalControlState(in_c);
    hc->Process();
    cc->Process();
    host.Update();
    client.Update();
    if (!host_transitioned && hc->State() == kStateGame) {
      host_transitioned = true;
    }
    if (!client_transitioned && cc->State() == kStateGame) {
      client_transitioned = true;
    }
    std::this_thread::sleep_for(std::chrono::microseconds(200));
  };

  // Drive both peers through weapon select: 6× Down (each press toggled
  // off to make a clean rising edge), a short idle pad, then Fire.
  std::vector<uint8_t> ws_script;
  for (int i = 0; i < 6; ++i) {
    ws_script.push_back(kBitDown);
    ws_script.push_back(0);
  }
  ws_script.push_back(0);
  ws_script.push_back(kBitFire);
  ws_script.push_back(0);
  for (uint8_t const kB : ws_script) {
    run_tick(kB, kB);
  }
  for (int i = 0; i < 400 && !(host_transitioned && client_transitioned); ++i) {
    run_tick(0, 0);
  }
  REQUIRE(host_transitioned);
  REQUIRE(client_transitioned);

  // Play the match out with aggressive combat until both controllers
  // finalize (Process() returns false once the kStateGameEnded fade has
  // drained). hc drives worm 0, cc drives worm 1.
  Rand rng_h(0xDEAD1234);
  Rand rng_c(0xDEAD1234U ^ 0x55555555U);
  bool host_done = false;
  bool client_done = false;
  constexpr int kMaxTicks = 40000;
  int ticks = 0;
  for (; ticks < kMaxTicks && !(host_done && client_done); ++ticks) {
    uint8_t const kInH = host_done ? uint8_t{0} : CombatInput(rng_h, 0);
    uint8_t const kInC = client_done ? uint8_t{0} : CombatInput(rng_c, 1);
    hc->SetLocalControlState(kInH);
    cc->SetLocalControlState(kInC);
    if (!host_done && !hc->Process()) {
      host_done = true;
    }
    if (!client_done && !cc->Process()) {
      client_done = true;
    }
    host.Update();
    client.Update();
    std::this_thread::sleep_for(std::chrono::microseconds(200));
  }

  INFO("ticks=" << ticks);
  REQUIRE(host_done);
  REQUIRE(client_done);
  REQUIRE(hc->State() == kStateGameEnded);
  REQUIRE(cc->State() == kStateGameEnded);
  REQUIRE(!host.DesyncDetected());
  REQUIRE(!client.DesyncDetected());
}
