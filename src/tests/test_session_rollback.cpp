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
  FsNode tcRoot;

  Env() {
    precomputeTables();
    common = std::make_shared<Common>();
    tcRoot = FsNode("data") / "TC" / "openliero";
    common->load(tcRoot);
    settings = std::make_shared<Settings>();
    settings->lives = 10;
    settings->loadingTime = 0;
    settings->loadChange = true;
    settings->randomLevel = true;
    settings->gameMode = Settings::GMKillEmAll;
  }
};

template <typename Pred>
bool pollUntil(NetSession& a, NetSession& b, Pred pred, int maxMs = 5000) {
  auto deadline = std::chrono::steady_clock::now() +
                  std::chrono::milliseconds(maxMs);
  while (!pred() && std::chrono::steady_clock::now() < deadline) {
    a.update();
    b.update();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return pred();
}

}  // namespace

TEST_CASE("NetSession reaches Playing and runs ticks", "[session][rollback]") {
  Env e;

  NetSession host(e.common, e.settings, e.tcRoot);
  NetSession client(e.common, e.settings, e.tcRoot);

  REQUIRE(host.hostGame(0));
  uint16_t port = host.transport().listeningPort();
  REQUIRE(client.joinGame("127.0.0.1", port));

  bool ready = pollUntil(host, client, [&]() {
    return host.sessionState() == NetSession::Playing &&
           client.sessionState() == NetSession::Playing;
  });
  REQUIRE(ready);

  REQUIRE(host.rollbackController() != nullptr);
  REQUIRE(client.rollbackController() != nullptr);

  // Both peers seeded the same RNG via handshake — identical game
  // state at frame 0.
  REQUIRE(host.rollbackController()->game.rand ==
          client.rollbackController()->game.rand);

  // Run a few sim ticks. Process pumps the controller (which sends
  // input batches via the wired NetTransport callback), and
  // session.update() polls the transport so received batches reach
  // the other controller. With zero loopback jitter prediction
  // should never trip a rollback.
  host.rollbackController()->focus();
  client.rollbackController()->focus();
  for (int i = 0; i < 50; ++i) {
    host.rollbackController()->process();
    client.rollbackController()->process();
    host.update();
    client.update();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  REQUIRE(host.rollbackController()->rollbackCount() == 0);
  REQUIRE(client.rollbackController()->rollbackCount() == 0);
  REQUIRE(host.rollbackController()->currentFrame() > 0);
  // With inputDelay=1 the peers can be up to kFrameAdvantage frames
  // apart at any snapshot — the frame-advantage stall bounds the gap
  // but doesn't guarantee frame-by-frame parity mid-loop.
  int32_t gap = static_cast<int32_t>(host.rollbackController()->currentFrame()) -
                static_cast<int32_t>(client.rollbackController()->currentFrame());
  REQUIRE(std::abs(gap) <= RollbackController::kFrameAdvantage);
  REQUIRE(!host.desyncDetected());
  REQUIRE(!client.desyncDetected());
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
  e.settings->inputDelay = 1;
  e.settings->selectBotWeapons = 0;

  auto clientSettings = std::make_shared<Settings>(*e.settings);
  // Default copy ctor shallow-copies the wormSettings shared_ptrs;
  // give each peer its own copies so single-process weapon-select
  // mutations on one side don't bleed into the other.
  for (auto& ws : clientSettings->wormSettings) {
    if (ws) ws = std::make_shared<WormSettings>(*ws);
  }

  NetSession host(e.common, e.settings, e.tcRoot);
  NetSession client(e.common, clientSettings, e.tcRoot);

  REQUIRE(host.hostGame(0));
  uint16_t port = host.transport().listeningPort();
  REQUIRE(client.joinGame("127.0.0.1", port));

  bool ready = pollUntil(host, client, [&]() {
    return host.sessionState() == NetSession::Playing &&
           client.sessionState() == NetSession::Playing;
  });
  REQUIRE(ready);

  auto* hc = host.rollbackController();
  auto* cc = client.rollbackController();
  REQUIRE(hc != nullptr);
  REQUIRE(cc != nullptr);

  hc->focus();
  cc->focus();

  constexpr uint8_t BIT_DOWN = uint8_t{1} << Worm::Down;
  constexpr uint8_t BIT_FIRE = uint8_t{1} << Worm::Fire;

  // Same script the controller-level test uses: 6× Down (each press is
  // on/off to produce a clean rising edge), a short idle pad, then a
  // single Fire press.
  std::vector<uint8_t> script;
  for (int i = 0; i < 6; ++i) {
    script.push_back(BIT_DOWN);
    script.push_back(0);
  }
  script.push_back(0);
  script.push_back(BIT_FIRE);
  script.push_back(0);

  uint32_t hostTransitionFrame = 0;
  uint32_t clientTransitionFrame = 0;
  bool hostTransitioned = false;
  bool clientTransitioned = false;

  auto runTick = [&](uint8_t inByte) {
    hc->setLocalControlState(inByte);
    cc->setLocalControlState(inByte);
    bool hostInWs = !hostTransitioned;
    bool clientInWs = !clientTransitioned;
    hc->process();
    cc->process();
    host.update();
    client.update();
    if (hostInWs && hc->gameState() == StateGame) {
      hostTransitioned = true;
      hostTransitionFrame = hc->currentFrame();
    }
    if (clientInWs && cc->gameState() == StateGame) {
      clientTransitioned = true;
      clientTransitionFrame = cc->currentFrame();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  };

  for (uint8_t inByte : script) runTick(inByte);
  // Idle tail to let promote loops drain and the transition fire on
  // both peers under loopback's small but non-zero RTT.
  for (int i = 0; i < 200 && !(hostTransitioned && clientTransitioned); ++i) {
    runTick(0);
  }

  REQUIRE(hostTransitioned);
  REQUIRE(clientTransitioned);
  REQUIRE(hostTransitionFrame == clientTransitionFrame);

  // Both peers picked the same weapons.
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < Settings::selectableWeapons; ++j) {
      REQUIRE(hc->game.worms[i]->settings->weapons[j] ==
              cc->game.worms[i]->settings->weapons[j]);
    }
  }

  REQUIRE(!host.desyncDetected());
  REQUIRE(!client.desyncDetected());
}

// End-to-end session test exercising the full WS→game boundary plus a
// long-haul game-phase run. Catches the asymmetric-WS-simFrame
// regression that only surfaces after both peers transition through
// real WS and keep producing checksums for several seconds.
TEST_CASE(
    "Rollback session runs ≥5s game-phase post-WS without desync",
    "[session][rollback]") {
  Env e;
  e.settings->inputDelay = 1;
  e.settings->selectBotWeapons = 0;

  auto clientSettings = std::make_shared<Settings>(*e.settings);
  for (auto& ws : clientSettings->wormSettings) {
    if (ws) ws = std::make_shared<WormSettings>(*ws);
  }

  NetSession host(e.common, e.settings, e.tcRoot);
  NetSession client(e.common, clientSettings, e.tcRoot);

  REQUIRE(host.hostGame(0));
  uint16_t port = host.transport().listeningPort();
  REQUIRE(client.joinGame("127.0.0.1", port));

  bool ready = pollUntil(host, client, [&]() {
    return host.sessionState() == NetSession::Playing &&
           client.sessionState() == NetSession::Playing;
  });
  REQUIRE(ready);

  auto* hc = host.rollbackController();
  auto* cc = client.rollbackController();
  REQUIRE(hc != nullptr);
  REQUIRE(cc != nullptr);

  hc->focus();
  cc->focus();

  constexpr uint8_t BIT_DOWN = uint8_t{1} << Worm::Down;
  constexpr uint8_t BIT_FIRE = uint8_t{1} << Worm::Fire;

  std::vector<uint8_t> wsScript;
  for (int i = 0; i < 6; ++i) {
    wsScript.push_back(BIT_DOWN);
    wsScript.push_back(0);
  }
  wsScript.push_back(0);
  wsScript.push_back(BIT_FIRE);
  wsScript.push_back(0);

  bool hostTransitioned = false, clientTransitioned = false;
  uint32_t hostTransitionFrame = 0, clientTransitionFrame = 0;

  auto runTick = [&](uint8_t inByte) {
    hc->setLocalControlState(inByte);
    cc->setLocalControlState(inByte);
    bool hostInWs = !hostTransitioned;
    bool clientInWs = !clientTransitioned;
    hc->process();
    cc->process();
    host.update();
    client.update();
    if (hostInWs && hc->gameState() == StateGame) {
      hostTransitioned = true;
      hostTransitionFrame = hc->currentFrame();
    }
    if (clientInWs && cc->gameState() == StateGame) {
      clientTransitioned = true;
      clientTransitionFrame = cc->currentFrame();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  };

  // Drive WS to completion.
  for (uint8_t b : wsScript) runTick(b);
  for (int i = 0; i < 200 && !(hostTransitioned && clientTransitioned); ++i) {
    runTick(0);
  }
  REQUIRE(hostTransitioned);
  REQUIRE(clientTransitioned);
  // Both peers enter game phase at simFrame=0 (post-WS reset).
  REQUIRE(hostTransitionFrame == 0);
  REQUIRE(clientTransitionFrame == 0);

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
    if ((i / 20) % 3 == 0) in |= BIT_FIRE;
    if ((i / 10) % 5 == 0) in |= (uint8_t{1} << Worm::Right);
    runTick(in);
  }

  // Idle drain so both peers' confirmation chains catch up. With the
  // frame-advantage clamp keeping the gap ≤ kFrameAdvantage, the
  // overlap window between the two rings should hold several confirmed
  // frames after the drain.
  for (int i = 0; i < 64; ++i) {
    runTick(0);
  }

  REQUIRE(hc->currentFrame() > 0);
  REQUIRE(cc->currentFrame() > 0);

  int32_t gap = static_cast<int32_t>(hc->currentFrame()) -
                static_cast<int32_t>(cc->currentFrame());
  REQUIRE(std::abs(gap) <= RollbackController::kFrameAdvantage);

  // Headline: the desync detector did NOT fire across the run.
  REQUIRE(!host.desyncDetected());
  REQUIRE(!client.desyncDetected());

  // Slot-level checksum agreement at a frame both peers have fully
  // confirmed. Pick the most recent frame that's resident in both
  // rings; under loopback both should comfortably hold the last
  // kMaxRollback+1 confirmed frames.
  rollback::RollbackBuffer const& bufH = hc->rollbackBuffer();
  rollback::RollbackBuffer const& bufC = cc->rollbackBuffer();
  int compareFrame = std::min(bufH.newestFrame(), bufC.newestFrame());
  REQUIRE(compareFrame > 0);
  auto* slotH = const_cast<rollback::RollbackBuffer&>(bufH).find(compareFrame);
  auto* slotC = const_cast<rollback::RollbackBuffer&>(bufC).find(compareFrame);
  REQUIRE(slotH != nullptr);
  REQUIRE(slotC != nullptr);
  REQUIRE(slotH->checksum == slotC->checksum);
}

TEST_CASE("Per-worm stats hooks fire on the shadow on both peers",
          "[session][rollback][stats]") {
  // Tighter regression: even if `frame` ticks, the per-worm hooks
  // (shot, damageDealt, …) used to be gated by speculative too, so the
  // client's NormalStatsRecorder stayed empty. The shadow's recorder is
  // unconditionally non-speculative, so firing weapons through the
  // pipeline should bump shot counts on both peers.
  Env e;
  e.settings->inputDelay = 1;

  auto clientSettings = std::make_shared<Settings>(*e.settings);
  for (auto& ws : clientSettings->wormSettings) {
    if (ws) ws = std::make_shared<WormSettings>(*ws);
  }

  NetSession host(e.common, e.settings, e.tcRoot);
  NetSession client(e.common, clientSettings, e.tcRoot);

  REQUIRE(host.hostGame(0));
  uint16_t port = host.transport().listeningPort();
  REQUIRE(client.joinGame("127.0.0.1", port));

  bool ready = pollUntil(host, client, [&]() {
    return host.sessionState() == NetSession::Playing &&
           client.sessionState() == NetSession::Playing;
  });
  REQUIRE(ready);

  auto* hc = host.rollbackController();
  auto* cc = client.rollbackController();
  REQUIRE(hc); REQUIRE(cc);
  hc->focus();
  cc->focus();

  constexpr uint8_t BIT_DOWN = uint8_t{1} << Worm::Down;
  constexpr uint8_t BIT_FIRE = uint8_t{1} << Worm::Fire;

  auto runTick = [&](uint8_t inA, uint8_t inB) {
    hc->setLocalControlState(inA);
    cc->setLocalControlState(inB);
    host.update();
    client.update();
    hc->process();
    cc->process();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  };

  // Drive WS to completion.
  for (int i = 0; i < 6; ++i) {
    runTick(BIT_DOWN, BIT_DOWN);
    runTick(0, 0);
  }
  runTick(0, 0);
  runTick(BIT_FIRE, BIT_FIRE);
  runTick(0, 0);
  for (int i = 0; i < 200 &&
       !(hc->gameState() == StateGame && cc->gameState() == StateGame); ++i) {
    runTick(0, 0);
  }
  REQUIRE(hc->gameState() == StateGame);
  REQUIRE(cc->gameState() == StateGame);

  // First ready up both worms with a single Fire press (rising edge
  // sets ready=true for doRespawning), then let them spawn.
  runTick(BIT_FIRE, BIT_FIRE);
  for (int i = 0; i < 5; ++i) runTick(0, 0);

  // Pulse Fire so the rising edge fires every other tick. Held-Fire
  // wouldn't work — edge detection only counts the press.
  for (int i = 0; i < 400; ++i) {
    runTick((i & 1) ? BIT_FIRE : 0, (i & 1) ? BIT_FIRE : 0);
  }
  // Idle drain.
  for (int i = 0; i < 30; ++i) runTick(0, 0);

  auto* hostStats =
      dynamic_cast<NormalStatsRecorder*>(hc->statsGame()->statsRecorder.get());
  auto* clientStats =
      dynamic_cast<NormalStatsRecorder*>(cc->statsGame()->statsRecorder.get());
  REQUIRE(hostStats);
  REQUIRE(clientStats);

  auto totalShots = [](NormalStatsRecorder* s) {
    int total = 0;
    for (int w = 0; w < 2; ++w)
      for (int wp = 0; wp < 40; ++wp)
        total += s->worms[w].weapons[wp].potentialHits +
                 s->worms[w].weapons[wp].actualHits;
    return total;
  };

  // Shadow recorded shots on both peers, not just the host.
  REQUIRE(totalShots(hostStats) > 0);
  REQUIRE(totalShots(clientStats) > 0);
}

TEST_CASE("Stats accumulate on both peers across a rollback game",
          "[session][rollback][stats]") {
  // Regression: the live game runs frames speculatively under rollback,
  // so NormalStatsRecorder::tick was skipped every frame on the peer
  // whose forward path always fell into the predicted branch. The fix
  // routes stats through the shadow Game (one tick per confirmed frame).
  // Verify that statsGame()'s NormalStatsRecorder accumulates `frame`,
  // sets `gameTime` after finish(), and works symmetrically on host AND
  // client.
  Env e;
  e.settings->inputDelay = 1;

  auto clientSettings = std::make_shared<Settings>(*e.settings);
  for (auto& ws : clientSettings->wormSettings) {
    if (ws) ws = std::make_shared<WormSettings>(*ws);
  }

  NetSession host(e.common, e.settings, e.tcRoot);
  NetSession client(e.common, clientSettings, e.tcRoot);

  REQUIRE(host.hostGame(0));
  uint16_t port = host.transport().listeningPort();
  REQUIRE(client.joinGame("127.0.0.1", port));

  bool ready = pollUntil(host, client, [&]() {
    return host.sessionState() == NetSession::Playing &&
           client.sessionState() == NetSession::Playing;
  });
  REQUIRE(ready);

  auto* hc = host.rollbackController();
  auto* cc = client.rollbackController();
  REQUIRE(hc != nullptr);
  REQUIRE(cc != nullptr);

  hc->focus();
  cc->focus();

  constexpr uint8_t BIT_DOWN = uint8_t{1} << Worm::Down;
  constexpr uint8_t BIT_FIRE = uint8_t{1} << Worm::Fire;

  auto runTick = [&](uint8_t inByte) {
    hc->setLocalControlState(inByte);
    cc->setLocalControlState(inByte);
    host.update();
    client.update();
    hc->process();
    cc->process();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  };

  // Drive both peers through WS into the game phase.
  std::vector<uint8_t> wsScript;
  for (int i = 0; i < 6; ++i) {
    wsScript.push_back(BIT_DOWN);
    wsScript.push_back(0);
  }
  wsScript.push_back(0);
  wsScript.push_back(BIT_FIRE);
  wsScript.push_back(0);
  for (uint8_t b : wsScript) runTick(b);
  for (int i = 0; i < 200 &&
       !(hc->gameState() == StateGame && cc->gameState() == StateGame); ++i) {
    runTick(0);
  }
  REQUIRE(hc->gameState() == StateGame);
  REQUIRE(cc->gameState() == StateGame);

  // Play out a bit of game phase.
  for (int i = 0; i < 200; ++i) runTick(0);

  // The shadow Game (statsGame()) is where stats live now. The live
  // game's recorder is the base no-op class — the dynamic_cast below
  // would fail on it, which is the *intended* contract.
  auto* hostStats =
      dynamic_cast<NormalStatsRecorder*>(hc->statsGame()->statsRecorder.get());
  auto* clientStats =
      dynamic_cast<NormalStatsRecorder*>(cc->statsGame()->statsRecorder.get());
  REQUIRE(hostStats != nullptr);
  REQUIRE(clientStats != nullptr);
  REQUIRE(hc->statsGame() != hc->currentGame());  // shadow, not live
  REQUIRE(cc->statsGame() != cc->currentGame());

  // Live recorders are the no-op base class — dynamic_cast fails.
  REQUIRE(dynamic_cast<NormalStatsRecorder*>(
              hc->currentGame()->statsRecorder.get()) == nullptr);
  REQUIRE(dynamic_cast<NormalStatsRecorder*>(
              cc->currentGame()->statsRecorder.get()) == nullptr);

  // Both shadows should have ticked far past 0 — the shadow runs once
  // per confirmed frame and confirmation tracks within a few frames of
  // simFrame under loopback.
  REQUIRE(hostStats->frame > 100);
  REQUIRE(clientStats->frame > 100);

  // Host and client should agree closely on frame count — the shadow's
  // counter is keyed to confirmedSimFrame_ which is the same on both
  // peers within a few-frame window.
  int frameGap = std::abs(hostStats->frame - clientStats->frame);
  REQUIRE(frameGap <= 10);

  // Pause + EndMatch flow: host pauses, picks END MATCH.
  host.sendPause();
  for (int i = 0; i < 10; ++i) runTick(0);
  REQUIRE(cc->isPaused());

  host.sendResume();
  host.transport().sendEndMatch();
  hc->endMatch();

  bool hostDone = false, clientDone = false;
  for (int i = 0; i < 200 && !(hostDone && clientDone); ++i) {
    hc->setLocalControlState(0);
    cc->setLocalControlState(0);
    if (!hostDone && !hc->process()) hostDone = true;
    if (!clientDone && !cc->process()) clientDone = true;
    host.update();
    client.update();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  REQUIRE(hostDone);
  REQUIRE(clientDone);

  // Both peers landed in StateGameEnded → finish() called on shadow →
  // gameTime now reflects the confirmed frame count.
  REQUIRE(hc->gameState() == StateGameEnded);
  REQUIRE(cc->gameState() == StateGameEnded);
  REQUIRE(hostStats->gameTime > 0);
  REQUIRE(clientStats->gameTime > 0);
  // gameTime is what gamePlayState checks to decide stats-vs-menu —
  // gating the symptom the user reported.
  int gtGap = std::abs(hostStats->gameTime - clientStats->gameTime);
  REQUIRE(gtGap <= 10);
}

TEST_CASE("Host's inputDelay syncs to the client over MatchSettings",
          "[session][rollback]") {
  Env e;
  e.settings->inputDelay = 2;

  // Client starts with a different inputDelay; host's value must
  // overwrite it via MatchSettings before tryStartGame builds the
  // controller.
  auto clientSettings = std::make_shared<Settings>(*e.settings);
  clientSettings->inputDelay = 5;

  NetSession host(e.common, e.settings, e.tcRoot);
  NetSession client(e.common, clientSettings, e.tcRoot);

  REQUIRE(host.hostGame(0));
  uint16_t port = host.transport().listeningPort();
  REQUIRE(client.joinGame("127.0.0.1", port));

  bool ready = pollUntil(host, client, [&]() {
    return host.sessionState() == NetSession::Playing &&
           client.sessionState() == NetSession::Playing;
  });
  REQUIRE(ready);

  REQUIRE(clientSettings->inputDelay == 2);
  REQUIRE(client.rollbackController() != nullptr);
}

TEST_CASE("Input batches received pre-Playing reach controller post-Playing",
          "[session][rollback][regression]") {
  Env e;
  e.settings->inputDelay = 1;

  NetSession host(e.common, e.settings, e.tcRoot);
  NetSession client(e.common, e.settings, e.tcRoot);

  REQUIRE(host.hostGame(0));
  uint16_t port = host.transport().listeningPort();
  REQUIRE(client.joinGame("127.0.0.1", port));

  REQUIRE(client.sessionState() != NetSession::Playing);
  REQUIRE(client.rollbackController() == nullptr);

  // Same entry point a real ENet packet would hit, while
  // client.rollbackPtr_ is still null.
  uint8_t earlyInputs[8] = {0};
  client.transport().onRemoteInputBatch(
      /*generation=*/0, /*baseFrame=*/0, /*count=*/8, earlyInputs,
      /*remoteLocalFrame=*/7);

  bool reached = pollUntil(host, client, [&]() {
    return host.sessionState() == NetSession::Playing &&
           client.sessionState() == NetSession::Playing;
  });
  REQUIRE(reached);
  REQUIRE(client.rollbackController() != nullptr);

  // No rollback.process() on either side during the handshake, so the
  // only remote input the client's confirm chain can see is the
  // pre-Playing injection above.
  auto* rb = client.rollbackController();
  rb->focus();
  for (int i = 0; i < 16; ++i) {
    rb->setLocalControlState(0);
    rb->process();
  }

  INFO("client simFrame=" << rb->currentFrame()
       << " confirmedFrame=" << rb->confirmedFrame());
  REQUIRE(rb->confirmedFrame() >= 7);
}

