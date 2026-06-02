// NetSession integration with RollbackController.
//
// Stands two NetSession instances up over loopback, drives the
// handshake → settings → mapdata → playing transition, and runs a few
// simulation ticks. The peers must stay frame-locked enough that a
// clean (zero-jitter) loopback never triggers a rollback or desync.

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <thread>

#include <algorithm>
#include <cstdlib>

#include "game.hpp"
#include "math.hpp"
#include "net/session.hpp"
#include "rollback/buffer.hpp"

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
    settings->lives = 10;
    settings->loading_time = 0;
    settings->load_change = true;
    settings->random_level = true;
    settings->game_mode = Settings::kGmKillEmAll;
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

}  // namespace

TEST_CASE("NetSession reaches Playing and runs ticks", "[session][rollback]") {
  Env e;

  NetSession host(e.common, e.settings, e.tc_root);
  NetSession client(e.common, e.settings, e.tc_root);

  REQUIRE(host.HostGame(0));
  uint16_t port = host.Transport().ListeningPort();
  REQUIRE(client.JoinGame("127.0.0.1", port));

  bool ready = PollUntil(host, client, [&]() {
    return host.State() == NetSession::kPlaying && client.State() == NetSession::kPlaying;
  });
  REQUIRE(ready);

  REQUIRE(host.Rollback() != nullptr);
  REQUIRE(client.Rollback() != nullptr);

  // Both peers seeded the same RNG via handshake — identical game
  // state at frame 0.
  REQUIRE(host.Rollback()->game.rand == client.Rollback()->game.rand);

  // Run a few sim ticks. Process pumps the controller (which sends
  // input batches via the wired NetTransport callback), and
  // session.update() polls the transport so received batches reach
  // the other controller. With zero loopback jitter prediction
  // should never trip a rollback.
  host.Rollback()->Focus();
  client.Rollback()->Focus();
  for (int i = 0; i < 50; ++i) {
    host.Rollback()->Process();
    client.Rollback()->Process();
    host.Update();
    client.Update();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  REQUIRE(host.Rollback()->RollbackCount() == 0);
  REQUIRE(client.Rollback()->RollbackCount() == 0);
  REQUIRE(host.Rollback()->CurrentFrame() > 0);
  // With inputDelay=1 the peers can be up to kFrameAdvantage frames
  // apart at any snapshot — the frame-advantage stall bounds the gap
  // but doesn't guarantee frame-by-frame parity mid-loop.
  int32_t gap = static_cast<int32_t>(host.Rollback()->CurrentFrame()) -
                static_cast<int32_t>(client.Rollback()->CurrentFrame());
  REQUIRE(std::abs(gap) <= RollbackController::kFrameAdvantage);
  REQUIRE(!host.DesyncDetected());
  REQUIRE(!client.DesyncDetected());
}

TEST_CASE("Rollback weapon select transitions to game over a real session",
          "[session][rollback][weapsel]") {
  // End-to-end version of test_rollback_weapsel's "jitter" case: spin up
  // two NetSessions in rollback mode over loopback ENet, drive both
  // peers' navigation inputs through weapon select, and confirm both
  // sides transition to StateGame at the same simFrame. Guards the
  // weapon-select-rollback integration with the session/transport layer
  // (not just the controller in isolation).
  Env e;
  e.settings->input_delay = 1;
  e.settings->select_bot_weapons = 0;

  auto client_settings = std::make_shared<Settings>(*e.settings);
  // Default copy ctor shallow-copies the wormSettings shared_ptrs;
  // give each peer its own copies so single-process weapon-select
  // mutations on one side don't bleed into the other.
  for (auto& ws : client_settings->worm_settings) {
    if (ws) ws = std::make_shared<WormSettings>(*ws);
  }

  NetSession host(e.common, e.settings, e.tc_root);
  NetSession client(e.common, client_settings, e.tc_root);

  REQUIRE(host.HostGame(0));
  uint16_t port = host.Transport().ListeningPort();
  REQUIRE(client.JoinGame("127.0.0.1", port));

  bool ready = PollUntil(host, client, [&]() {
    return host.State() == NetSession::kPlaying && client.State() == NetSession::kPlaying;
  });
  REQUIRE(ready);

  auto* hc = host.Rollback();
  auto* cc = client.Rollback();
  REQUIRE(hc != nullptr);
  REQUIRE(cc != nullptr);

  hc->Focus();
  cc->Focus();

  constexpr uint8_t kBitDown = uint8_t{1} << Worm::kDown;
  constexpr uint8_t kBitFire = uint8_t{1} << Worm::kFire;

  // Same script the controller-level test uses: 6× Down (each press is
  // on/off to produce a clean rising edge), a short idle pad, then a
  // single Fire press.
  std::vector<uint8_t> script;
  for (int i = 0; i < 6; ++i) {
    script.push_back(kBitDown);
    script.push_back(0);
  }
  script.push_back(0);
  script.push_back(kBitFire);
  script.push_back(0);

  uint32_t host_transition_frame = 0;
  uint32_t client_transition_frame = 0;
  bool host_transitioned = false;
  bool client_transitioned = false;

  auto run_tick = [&](uint8_t in_byte) {
    hc->SetLocalControlState(in_byte);
    cc->SetLocalControlState(in_byte);
    bool host_in_ws = !host_transitioned;
    bool client_in_ws = !client_transitioned;
    hc->Process();
    cc->Process();
    host.Update();
    client.Update();
    if (host_in_ws && hc->State() == kStateGame) {
      host_transitioned = true;
      host_transition_frame = hc->CurrentFrame();
    }
    if (client_in_ws && cc->State() == kStateGame) {
      client_transitioned = true;
      client_transition_frame = cc->CurrentFrame();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  };

  for (uint8_t in_byte : script) run_tick(in_byte);
  // Idle tail to let promote loops drain and the transition fire on
  // both peers under loopback's small but non-zero RTT.
  for (int i = 0; i < 200 && !(host_transitioned && client_transitioned); ++i) {
    run_tick(0);
  }

  REQUIRE(host_transitioned);
  REQUIRE(client_transitioned);
  REQUIRE(host_transition_frame == client_transition_frame);

  // Both peers picked the same weapons.
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < Settings::kSelectableWeapons; ++j) {
      REQUIRE(hc->game.worms[i]->settings->weapons[j] == cc->game.worms[i]->settings->weapons[j]);
    }
  }

  REQUIRE(!host.DesyncDetected());
  REQUIRE(!client.DesyncDetected());
}

// End-to-end session test exercising the full WS→game boundary plus a
// long-haul game-phase run. Catches the asymmetric-WS-simFrame
// regression that only surfaces after both peers transition through
// real WS and keep producing checksums for several seconds.
TEST_CASE("Rollback session runs ≥5s game-phase post-WS without desync", "[session][rollback]") {
  Env e;
  e.settings->input_delay = 1;
  e.settings->select_bot_weapons = 0;

  auto client_settings = std::make_shared<Settings>(*e.settings);
  for (auto& ws : client_settings->worm_settings) {
    if (ws) ws = std::make_shared<WormSettings>(*ws);
  }

  NetSession host(e.common, e.settings, e.tc_root);
  NetSession client(e.common, client_settings, e.tc_root);

  REQUIRE(host.HostGame(0));
  uint16_t port = host.Transport().ListeningPort();
  REQUIRE(client.JoinGame("127.0.0.1", port));

  bool ready = PollUntil(host, client, [&]() {
    return host.State() == NetSession::kPlaying && client.State() == NetSession::kPlaying;
  });
  REQUIRE(ready);

  auto* hc = host.Rollback();
  auto* cc = client.Rollback();
  REQUIRE(hc != nullptr);
  REQUIRE(cc != nullptr);

  hc->Focus();
  cc->Focus();

  constexpr uint8_t kBitDown = uint8_t{1} << Worm::kDown;
  constexpr uint8_t kBitFire = uint8_t{1} << Worm::kFire;

  std::vector<uint8_t> ws_script;
  for (int i = 0; i < 6; ++i) {
    ws_script.push_back(kBitDown);
    ws_script.push_back(0);
  }
  ws_script.push_back(0);
  ws_script.push_back(kBitFire);
  ws_script.push_back(0);

  bool host_transitioned = false, client_transitioned = false;
  uint32_t host_transition_frame = 0, client_transition_frame = 0;

  auto run_tick = [&](uint8_t in_byte) {
    hc->SetLocalControlState(in_byte);
    cc->SetLocalControlState(in_byte);
    bool host_in_ws = !host_transitioned;
    bool client_in_ws = !client_transitioned;
    hc->Process();
    cc->Process();
    host.Update();
    client.Update();
    if (host_in_ws && hc->State() == kStateGame) {
      host_transitioned = true;
      host_transition_frame = hc->CurrentFrame();
    }
    if (client_in_ws && cc->State() == kStateGame) {
      client_transitioned = true;
      client_transition_frame = cc->CurrentFrame();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  };

  // Drive WS to completion.
  for (uint8_t b : ws_script) run_tick(b);
  for (int i = 0; i < 200 && !(host_transitioned && client_transitioned); ++i) {
    run_tick(0);
  }
  REQUIRE(host_transitioned);
  REQUIRE(client_transitioned);
  // Both peers enter game phase at simFrame=0 (post-WS reset).
  REQUIRE(host_transition_frame == 0);
  REQUIRE(client_transition_frame == 0);

  // Drive 5 seconds of game-phase ticks (~350 frames at 70 Hz). Use
  // 400 ticks for headroom — even if one peer stalls a few frames via
  // the frame-advantage clamp, both still cover well past 5s of sim.
  constexpr int kGameTicks = 400;
  for (int i = 0; i < kGameTicks; ++i) {
    // Light scripted input to exercise weapon fire + movement so the
    // checksum covers more than just the idle resting state. The exact
    // input doesn't matter — both peers run the same script, so any
    // mismatch must come from the sim path, not divergent inputs.
    uint8_t in = 0;
    if ((i / 20) % 3 == 0) in |= kBitFire;
    if ((i / 10) % 5 == 0) in |= (uint8_t{1} << Worm::kRight);
    run_tick(in);
  }

  // Idle drain so both peers' confirmation chains catch up. With the
  // frame-advantage clamp keeping the gap ≤ kFrameAdvantage, the
  // overlap window between the two rings should hold several confirmed
  // frames after the drain.
  for (int i = 0; i < 64; ++i) {
    run_tick(0);
  }

  REQUIRE(hc->CurrentFrame() > 0);
  REQUIRE(cc->CurrentFrame() > 0);

  int32_t gap = static_cast<int32_t>(hc->CurrentFrame()) - static_cast<int32_t>(cc->CurrentFrame());
  REQUIRE(std::abs(gap) <= RollbackController::kFrameAdvantage);

  // Headline: the desync detector did NOT fire across the run.
  REQUIRE(!host.DesyncDetected());
  REQUIRE(!client.DesyncDetected());

  // Slot-level checksum agreement at a frame both peers have fully
  // confirmed. Pick the most recent frame that's resident in both
  // rings; under loopback both should comfortably hold the last
  // kMaxRollback+1 confirmed frames.
  rollback::RollbackBuffer const& buf_h = hc->RollbackBuffer();
  rollback::RollbackBuffer const& buf_c = cc->RollbackBuffer();
  int compare_frame = std::min(buf_h.NewestFrame(), buf_c.NewestFrame());
  REQUIRE(compare_frame > 0);
  auto* slot_h = const_cast<rollback::RollbackBuffer&>(buf_h).Find(compare_frame);
  auto* slot_c = const_cast<rollback::RollbackBuffer&>(buf_c).Find(compare_frame);
  REQUIRE(slot_h != nullptr);
  REQUIRE(slot_c != nullptr);
  REQUIRE(slot_h->checksum == slot_c->checksum);
}

TEST_CASE("Per-worm stats hooks fire on the shadow on both peers", "[session][rollback][stats]") {
  // Tighter regression: even if `frame` ticks, the per-worm hooks
  // (shot, damageDealt, …) used to be gated by speculative too, so the
  // client's NormalStatsRecorder stayed empty. The shadow's recorder is
  // unconditionally non-speculative, so firing weapons through the
  // pipeline should bump shot counts on both peers.
  Env e;
  e.settings->input_delay = 1;

  auto client_settings = std::make_shared<Settings>(*e.settings);
  for (auto& ws : client_settings->worm_settings) {
    if (ws) ws = std::make_shared<WormSettings>(*ws);
  }

  NetSession host(e.common, e.settings, e.tc_root);
  NetSession client(e.common, client_settings, e.tc_root);

  REQUIRE(host.HostGame(0));
  uint16_t port = host.Transport().ListeningPort();
  REQUIRE(client.JoinGame("127.0.0.1", port));

  bool ready = PollUntil(host, client, [&]() {
    return host.State() == NetSession::kPlaying && client.State() == NetSession::kPlaying;
  });
  REQUIRE(ready);

  auto* hc = host.Rollback();
  auto* cc = client.Rollback();
  REQUIRE(hc);
  REQUIRE(cc);
  hc->Focus();
  cc->Focus();

  constexpr uint8_t kBitDown = uint8_t{1} << Worm::kDown;
  constexpr uint8_t kBitFire = uint8_t{1} << Worm::kFire;

  auto run_tick = [&](uint8_t in_a, uint8_t in_b) {
    hc->SetLocalControlState(in_a);
    cc->SetLocalControlState(in_b);
    host.Update();
    client.Update();
    hc->Process();
    cc->Process();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  };

  // Drive WS to completion.
  for (int i = 0; i < 6; ++i) {
    run_tick(kBitDown, kBitDown);
    run_tick(0, 0);
  }
  run_tick(0, 0);
  run_tick(kBitFire, kBitFire);
  run_tick(0, 0);
  for (int i = 0; i < 200 && !(hc->State() == kStateGame && cc->State() == kStateGame); ++i) {
    run_tick(0, 0);
  }
  REQUIRE(hc->State() == kStateGame);
  REQUIRE(cc->State() == kStateGame);

  // First ready up both worms with a single Fire press (rising edge
  // sets ready=true for doRespawning), then let them spawn.
  run_tick(kBitFire, kBitFire);
  for (int i = 0; i < 5; ++i) run_tick(0, 0);

  // Pulse Fire so the rising edge fires every other tick. Held-Fire
  // wouldn't work — edge detection only counts the press.
  for (int i = 0; i < 400; ++i) {
    run_tick((i & 1) ? kBitFire : 0, (i & 1) ? kBitFire : 0);
  }
  // Idle drain.
  for (int i = 0; i < 30; ++i) run_tick(0, 0);

  auto* host_stats = dynamic_cast<NormalStatsRecorder*>(hc->StatsGame()->stats_recorder.get());
  auto* client_stats = dynamic_cast<NormalStatsRecorder*>(cc->StatsGame()->stats_recorder.get());
  REQUIRE(host_stats);
  REQUIRE(client_stats);

  auto total_shots = [](NormalStatsRecorder* s) {
    int total = 0;
    for (int w = 0; w < 2; ++w)
      for (int wp = 0; wp < 40; ++wp)
        total += s->worms[w].weapons[wp].potential_hits + s->worms[w].weapons[wp].actual_hits;
    return total;
  };

  // Shadow recorded shots on both peers, not just the host.
  REQUIRE(total_shots(host_stats) > 0);
  REQUIRE(total_shots(client_stats) > 0);
}

TEST_CASE("Stats accumulate on both peers across a rollback game", "[session][rollback][stats]") {
  // Regression: the live game runs frames speculatively under rollback,
  // so NormalStatsRecorder::tick was skipped every frame on the peer
  // whose forward path always fell into the predicted branch. The fix
  // routes stats through the shadow Game (one tick per confirmed frame).
  // Verify that statsGame()'s NormalStatsRecorder accumulates `frame`,
  // sets `gameTime` after finish(), and works symmetrically on host AND
  // client.
  Env e;
  e.settings->input_delay = 1;

  auto client_settings = std::make_shared<Settings>(*e.settings);
  for (auto& ws : client_settings->worm_settings) {
    if (ws) ws = std::make_shared<WormSettings>(*ws);
  }

  NetSession host(e.common, e.settings, e.tc_root);
  NetSession client(e.common, client_settings, e.tc_root);

  REQUIRE(host.HostGame(0));
  uint16_t port = host.Transport().ListeningPort();
  REQUIRE(client.JoinGame("127.0.0.1", port));

  bool ready = PollUntil(host, client, [&]() {
    return host.State() == NetSession::kPlaying && client.State() == NetSession::kPlaying;
  });
  REQUIRE(ready);

  auto* hc = host.Rollback();
  auto* cc = client.Rollback();
  REQUIRE(hc != nullptr);
  REQUIRE(cc != nullptr);

  hc->Focus();
  cc->Focus();

  constexpr uint8_t kBitDown = uint8_t{1} << Worm::kDown;
  constexpr uint8_t kBitFire = uint8_t{1} << Worm::kFire;

  auto run_tick = [&](uint8_t in_byte) {
    hc->SetLocalControlState(in_byte);
    cc->SetLocalControlState(in_byte);
    host.Update();
    client.Update();
    hc->Process();
    cc->Process();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  };

  // Drive both peers through WS into the game phase.
  std::vector<uint8_t> ws_script;
  for (int i = 0; i < 6; ++i) {
    ws_script.push_back(kBitDown);
    ws_script.push_back(0);
  }
  ws_script.push_back(0);
  ws_script.push_back(kBitFire);
  ws_script.push_back(0);
  for (uint8_t b : ws_script) run_tick(b);
  for (int i = 0; i < 200 && !(hc->State() == kStateGame && cc->State() == kStateGame); ++i) {
    run_tick(0);
  }
  REQUIRE(hc->State() == kStateGame);
  REQUIRE(cc->State() == kStateGame);

  // Play out a bit of game phase.
  for (int i = 0; i < 200; ++i) run_tick(0);

  // The shadow Game (statsGame()) is where stats live now. The live
  // game's recorder is the base no-op class — the dynamic_cast below
  // would fail on it, which is the *intended* contract.
  auto* host_stats = dynamic_cast<NormalStatsRecorder*>(hc->StatsGame()->stats_recorder.get());
  auto* client_stats = dynamic_cast<NormalStatsRecorder*>(cc->StatsGame()->stats_recorder.get());
  REQUIRE(host_stats != nullptr);
  REQUIRE(client_stats != nullptr);
  REQUIRE(hc->StatsGame() != hc->CurrentGame());  // shadow, not live
  REQUIRE(cc->StatsGame() != cc->CurrentGame());

  // Live recorders are the no-op base class — dynamic_cast fails.
  REQUIRE(dynamic_cast<NormalStatsRecorder*>(hc->CurrentGame()->stats_recorder.get()) == nullptr);
  REQUIRE(dynamic_cast<NormalStatsRecorder*>(cc->CurrentGame()->stats_recorder.get()) == nullptr);

  // Both shadows should have ticked far past 0 — the shadow runs once
  // per confirmed frame and confirmation tracks within a few frames of
  // simFrame under loopback.
  REQUIRE(host_stats->frame > 100);
  REQUIRE(client_stats->frame > 100);

  // Host and client should agree closely on frame count — the shadow's
  // counter is keyed to confirmedSimFrame_ which is the same on both
  // peers within a few-frame window.
  int frame_gap = std::abs(host_stats->frame - client_stats->frame);
  REQUIRE(frame_gap <= 10);

  // Pause + EndMatch flow: host pauses, picks END MATCH.
  host.SendPause();
  for (int i = 0; i < 10; ++i) run_tick(0);
  REQUIRE(cc->IsPaused());

  host.SendResume();
  host.Transport().SendEndMatch();
  hc->EndMatch();

  bool host_done = false, client_done = false;
  for (int i = 0; i < 200 && !(host_done && client_done); ++i) {
    hc->SetLocalControlState(0);
    cc->SetLocalControlState(0);
    if (!host_done && !hc->Process()) host_done = true;
    if (!client_done && !cc->Process()) client_done = true;
    host.Update();
    client.Update();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  REQUIRE(host_done);
  REQUIRE(client_done);

  // Both peers landed in StateGameEnded → finish() called on shadow →
  // gameTime now reflects the confirmed frame count.
  REQUIRE(hc->State() == kStateGameEnded);
  REQUIRE(cc->State() == kStateGameEnded);
  REQUIRE(host_stats->game_time > 0);
  REQUIRE(client_stats->game_time > 0);
  // gameTime is what gamePlayState checks to decide stats-vs-menu —
  // gating the symptom the user reported.
  int gt_gap = std::abs(host_stats->game_time - client_stats->game_time);
  REQUIRE(gt_gap <= 10);
}

TEST_CASE("Host's inputDelay syncs to the client over MatchSettings", "[session][rollback]") {
  Env e;
  e.settings->input_delay = 2;

  // Client starts with a different inputDelay; host's value must
  // overwrite it via MatchSettings before tryStartGame builds the
  // controller.
  auto client_settings = std::make_shared<Settings>(*e.settings);
  client_settings->input_delay = 5;

  NetSession host(e.common, e.settings, e.tc_root);
  NetSession client(e.common, client_settings, e.tc_root);

  REQUIRE(host.HostGame(0));
  uint16_t port = host.Transport().ListeningPort();
  REQUIRE(client.JoinGame("127.0.0.1", port));

  bool ready = PollUntil(host, client, [&]() {
    return host.State() == NetSession::kPlaying && client.State() == NetSession::kPlaying;
  });
  REQUIRE(ready);

  REQUIRE(client_settings->input_delay == 2);
  REQUIRE(client.Rollback() != nullptr);
}

TEST_CASE("Input batches received pre-Playing reach controller post-Playing",
          "[session][rollback][regression]") {
  Env e;
  e.settings->input_delay = 1;

  NetSession host(e.common, e.settings, e.tc_root);
  NetSession client(e.common, e.settings, e.tc_root);

  REQUIRE(host.HostGame(0));
  uint16_t port = host.Transport().ListeningPort();
  REQUIRE(client.JoinGame("127.0.0.1", port));

  REQUIRE(client.State() != NetSession::kPlaying);
  REQUIRE(client.Rollback() == nullptr);

  // Same entry point a real ENet packet would hit, while
  // client.rollbackPtr_ is still null.
  uint8_t early_inputs[8] = {0};
  client.Transport().on_remote_input_batch(
      /*generation=*/0, /*baseFrame=*/0, /*count=*/8, early_inputs,
      /*remoteLocalFrame=*/7);

  bool reached = PollUntil(host, client, [&]() {
    return host.State() == NetSession::kPlaying && client.State() == NetSession::kPlaying;
  });
  REQUIRE(reached);
  REQUIRE(client.Rollback() != nullptr);

  // No rollback.process() on either side during the handshake, so the
  // only remote input the client's confirm chain can see is the
  // pre-Playing injection above.
  auto* rb = client.Rollback();
  rb->Focus();
  for (int i = 0; i < 16; ++i) {
    rb->SetLocalControlState(0);
    rb->Process();
  }

  INFO("client simFrame=" << rb->CurrentFrame() << " confirmedFrame=" << rb->ConfirmedFrame());
  REQUIRE(rb->ConfirmedFrame() >= 7);
}
