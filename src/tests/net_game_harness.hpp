// Shared headless harness for full-game *network* integration tests
// (Layer A). Drives a pair of RollbackControllers through the in-memory
// JitterTransport (see jitter_transport.hpp) to a complete match.
//
// Sibling of game_harness.hpp: that header builds a bare Game for
// single-player runs; here each RollbackController owns its own Game, so
// MakeHeadlessGame cannot be reused. Only the builder + RunResult *style*
// is mirrored.
//
// Game-over oracle (plan Task 0): termination keys on the controller's
// State() == kStateGameEnded. The live game latches that at
// rollbackController.cpp's `game.IsGameOver()` check — the real
// production end-of-match transition. The shadow Game (StatsGame()) is
// deliberately NOT used as the oracle: it only advances up to
// confirmedSimFrame_, which stops the instant the live game latches
// kStateGameEnded, so it can freeze a few frames short of the game-over
// frame and never report it.
//
// Convergence is verified through the confirmed-frame checksum callbacks
// (SetChecksumCallback fires only on non-predicted frames), not by
// comparing frozen live state: under jitter the two peers can latch
// game-over on different (predicted) frames, so their frozen live
// checksums need not be bit-identical, but every *confirmed* frame they
// both ran must agree.
//
// Weapon selection is skipped (SetSkipWeaponSelection(true)); the WS /
// menu phase is covered by test_rollback_replay.cpp and
// test_session_rollback.cpp.
#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>

#include "controller/rollbackController.hpp"
#include "game.hpp"
#include "jitter_transport.hpp"
#include "math.hpp"
#include "mixer/player.hpp"

namespace rollback_test {

struct NetGameConfig {
  uint32_t world_seed{0xBEEF};
  int game_mode{Settings::kGmKillEmAll};
  int lives{1};
  int health{15};  // per-worm spawn health; 0 → keep WormSettings default
};

// The two controllers plus the env keeping their shared Common/Settings
// alive. Each controller owns its Game (rollback runs the live game
// speculatively and a confirmed-only shadow alongside it).
struct RollbackPair {
  std::shared_ptr<Common> common;
  std::shared_ptr<Settings> settings;
  std::unique_ptr<RollbackController> a;
  std::unique_ptr<RollbackController> b;
};

inline RollbackPair MakeRollbackPair(NetGameConfig const& cfg = {}) {
  PrecomputeTables();

  auto common = std::make_shared<Common>();
  FsNode const kTcRoot(FsNode("data") / "TC" / "openliero");
  common->load(kTcRoot);

  auto settings = std::make_shared<Settings>();
  settings->lives = cfg.lives;
  settings->loading_time = 0;
  settings->load_change = true;
  settings->random_level = true;
  settings->game_mode = cfg.game_mode;
  if (cfg.health > 0) {
    for (auto& ws : settings->worm_settings) {
      if (ws) {
        ws->health = cfg.health;
      }
    }
  }

  RollbackPair pair;
  pair.common = common;
  pair.settings = settings;
  pair.a = std::make_unique<RollbackController>(common, settings, 0);
  pair.b = std::make_unique<RollbackController>(common, settings, 1);
  pair.a->SetSkipWeaponSelection(/*skip=*/true);
  pair.b->SetSkipWeaponSelection(/*skip=*/true);
  // The frame-advantage stall is a time-sync clamp orthogonal to the
  // rollback algorithm; disable it so the peers freely run ahead and
  // exercise prediction, exactly as the rollback correctness/loss tests
  // do. Confirmation lag is still bounded by the ring (kMaxRollback).
  pair.a->SetFrameAdvantageEnabled(/*enabled=*/false);
  pair.b->SetFrameAdvantageEnabled(/*enabled=*/false);
  // Identical world RNG on both peers — without this the per-frame
  // checksums diverge from frame 0 and a mismatch reflects a harness
  // seeding bug, not a rollback-code bug.
  pair.a->game.rand.Seed(cfg.world_seed);
  pair.b->game.rand.Seed(cfg.world_seed);
  return pair;
}

struct NetRunResult {
  int frames_elapsed{0};          // scripted ticks executed before both ended
  bool reached_game_over{false};  // both peers latched kStateGameEnded
  bool desynced{false};           // a confirmed-frame checksum disagreed
  uint64_t compared_frames{0};    // confirmed frames compared (>0 ⇒ non-vacuous)
  uint64_t rollback_count_a{0};
  uint64_t rollback_count_b{0};
  uint32_t max_lag{0};  // max(simFrame - (confirmedFrame+1)) over both, post-warmup
};

// input_fn(peer_idx, frame) returns the 7-bit control byte for that
// peer's local worm (peer 0 → worm 0, peer 1 → worm 1). A peer that has
// already reached game-over is fed idle input.
using PairInputFn = std::function<uint8_t(int peer_idx, int frame)>;

inline NetRunResult RunPairToCompletion(RollbackPair& pair, JitterTransport& transport,
                                        PairInputFn const& input_fn, int max_frames = 200000) {
  RollbackController& a = *pair.a;
  RollbackController& b = *pair.b;

  NetRunResult res;

  // Confirmed-frame checksum exchange for desync detection. Store each
  // peer's emission keyed by frame; when both have produced a checksum
  // for the same confirmed frame, compare them.
  std::map<uint32_t, uint32_t> a_chk;
  std::map<uint32_t, uint32_t> b_chk;
  auto record = [&res](std::map<uint32_t, uint32_t>& mine, std::map<uint32_t, uint32_t>& other,
                       uint32_t frame, uint32_t chk) {
    mine[frame] = chk;
    auto it = other.find(frame);
    if (it != other.end()) {
      ++res.compared_frames;
      if (it->second != chk) {
        res.desynced = true;
      }
    }
  };
  a.SetChecksumCallback(
      [&](uint8_t /*gen*/, uint32_t f, uint32_t c) { record(a_chk, b_chk, f, c); });
  b.SetChecksumCallback(
      [&](uint8_t /*gen*/, uint32_t f, uint32_t c) { record(b_chk, a_chk, f, c); });

  a.SetInputCallbacks([&](uint8_t gen, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
    transport.SendAToB(gen, bf, c, in, lf);
  });
  b.SetInputCallbacks([&](uint8_t gen, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
    transport.SendBToA(gen, bf, c, in, lf);
  });

  a.Focus();
  b.Focus();

  // Pre-seed the input-delay window (default inputDelay = 3). These
  // bypass the transport so both peers start advancing immediately,
  // mirroring how a real session synchronises its starting frames.
  for (uint32_t f = 0; f < 3; ++f) {
    a.InjectRemoteInput(f, 0);
    b.InjectRemoteInput(f, 0);
  }

  auto deliver_a = [&](uint8_t gen, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
    a.InjectRemoteBatch(gen, bf, c, in, lf);
  };
  auto deliver_b = [&](uint8_t gen, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
    b.InjectRemoteBatch(gen, bf, c, in, lf);
  };

  int frame = 0;
  for (; frame < max_frames; ++frame) {
    bool const kAOver = a.State() == kStateGameEnded;
    bool const kBOver = b.State() == kStateGameEnded;
    if (kAOver && kBOver) {
      break;
    }

    a.SetLocalControlState(kAOver ? uint8_t{0} : input_fn(0, frame));
    b.SetLocalControlState(kBOver ? uint8_t{0} : input_fn(1, frame));
    a.Process();
    b.Process();
    transport.Tick(deliver_a, deliver_b);

    // Skip the warm-up window so it doesn't pollute the running maximum.
    if (frame > 50) {
      uint32_t const kLagA = a.CurrentFrame() - static_cast<uint32_t>(a.ConfirmedFrame() + 1);
      uint32_t const kLagB = b.CurrentFrame() - static_cast<uint32_t>(b.ConfirmedFrame() + 1);
      res.max_lag = std::max({res.max_lag, kLagA, kLagB});
    }
  }

  // Flush any in-flight tail packets and run a short idle drain so both
  // confirmation chains catch up and exchange their final checksums.
  transport.Flush(deliver_a, deliver_b);
  for (int i = 0; i < 16; ++i) {
    a.SetLocalControlState(0);
    b.SetLocalControlState(0);
    a.Process();
    b.Process();
    transport.Tick(deliver_a, deliver_b);
  }

  res.frames_elapsed = frame;
  res.reached_game_over = a.State() == kStateGameEnded && b.State() == kStateGameEnded;
  res.rollback_count_a = a.RollbackCount();
  res.rollback_count_b = b.RollbackCount();
  return res;
}

}  // namespace rollback_test
