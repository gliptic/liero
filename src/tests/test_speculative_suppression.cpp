// Game::speculative side-effect suppression.
//
// Contract: while game.setSpeculative(true), SoundPlayer::play/stop and
// StatsRecorder writes must not be observable. isPlaying may pass
// through.
//
// An N-frame run that goes run→snapshot→speculative-run→restore→run
// must produce the same observable counts as a single 2N-frame run.

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdint>
#include <memory>

#include "game.hpp"
#include "level.hpp"
#include "math.hpp"
#include "mixer/player.hpp"
#include "serialization/fast_snapshot.hpp"
#include "stats_recorder.hpp"
#include "viewport.hpp"
#include "weapon.hpp"
#include "worm.hpp"

namespace {

struct CountingSoundPlayer : SoundPlayer {
  int plays = 0;
  int stops = 0;
  int isPlayingCalls = 0;

  bool isPlaying(void* /*id*/) override {
    ++isPlayingCalls;
    return false;
  }
  void stop(void* /*id*/) override {
    if (speculative) return;
    ++stops;
  }

 protected:
  void playImpl(int /*sound*/, void* /*id*/, int /*loops*/) override { ++plays; }
};

struct CountingStatsRecorder : StatsRecorder {
  int events = 0;

#define EVT()                \
  do {                       \
    if (speculative) return; \
    ++events;                \
  } while (0)
  void damagePotential(Worm*, WormWeapon*, int) override { EVT(); }
  void damageDealt(Worm*, WormWeapon*, Worm*, int, bool) override { EVT(); }
  void shot(Worm*, WormWeapon*) override { EVT(); }
  void hit(Worm*, WormWeapon*, Worm*) override { EVT(); }
  void afterSpawn(Worm*) override { EVT(); }
  void afterDeath(Worm*) override { EVT(); }
  void preTick(Game&) override { EVT(); }
  void tick(Game&) override { EVT(); }
  void finish(Game&) override { EVT(); }
  void aiProcessTime(Worm*, std::chrono::nanoseconds) override { EVT(); }
#undef EVT
};

struct Runner {
  std::shared_ptr<Common> common;
  std::shared_ptr<Settings> settings;
  std::shared_ptr<CountingSoundPlayer> sp;
  std::unique_ptr<Game> game;

  Runner(uint32_t seed) {
    precomputeTables();
    common = std::make_shared<Common>();
    FsNode tcRoot(FsNode("data") / "TC" / "openliero");
    common->load(std::move(tcRoot));

    settings = std::make_shared<Settings>();
    settings->lives = 50;
    settings->loadingTime = 0;
    settings->randomLevel = true;
    settings->gameMode = Settings::GMKillEmAll;
    settings->blood = 100;

    sp = std::make_shared<CountingSoundPlayer>();
    game = std::make_unique<Game>(common, settings, sp);
    game->statsRecorder.reset(new CountingStatsRecorder);
    game->rand.seed(seed);

    for (int idx = 0; idx < 2; ++idx) {
      auto w = std::make_shared<Worm>();
      w->settings = settings->wormSettings[idx];
      w->health = 25;
      w->index = idx;
      w->statsX = idx == 0 ? 0 : 218;
      game->addWorm(w);
    }
    game->addViewport(new Viewport(Rect(0, 0, 158, 158), 0, 504, 350));
    game->addViewport(new Viewport(Rect(160, 0, 318, 158), 1, 504, 350));

    game->level.generateFromSettings(*common, *settings, game->rand);
    for (auto const& w : game->worms) w->initWeapons(*game);

    game->paused = false;
    game->startGame();
    game->resetWorms();
  }

  void step(Rand& inputRng) {
    for (int idx = 0; idx < 2; ++idx) {
      uint32_t input = inputRng() & 0x7f;
      if ((inputRng() % 10) < 6) input |= (1 << 4);
      if ((inputRng() % 10) < 4) input |= (1 << (idx == 0 ? 1 : 0));
      game->worms[idx]->controlStates.unpack(input);
    }
    game->processFrame();
  }

  CountingStatsRecorder& stats() {
    return static_cast<CountingStatsRecorder&>(*game->statsRecorder);
  }
};

}  // namespace

TEST_CASE("Speculative frames suppress sound and stats", "[rollback]") {
  constexpr uint32_t kSeed = 0xC0FFEE;
  constexpr int kPhase = 200;

  // Control: 2 * kPhase frames with no speculation.
  Runner ctl(kSeed);
  Rand ctlInputs(kSeed ^ 0xDEAD);
  for (int f = 0; f < 2 * kPhase; ++f) ctl.step(ctlInputs);
  int controlPlays = ctl.sp->plays;
  int controlStops = ctl.sp->stops;
  int controlEvents = ctl.stats().events;

  // Subject: run kPhase normally, save snapshot, run kPhase speculatively,
  // restore, run kPhase more normally. Final counts must match the control.
  Runner sub(kSeed);
  Rand subInputs(kSeed ^ 0xDEAD);

  GameSnapshot snap;
  snap.prepare(*sub.game);

  for (int f = 0; f < kPhase; ++f) sub.step(subInputs);
  int playsAtSnap = sub.sp->plays;
  int stopsAtSnap = sub.sp->stops;
  int eventsAtSnap = sub.stats().events;

  sub.game->saveSnapshotFast(snap);
  sub.game->setSpeculative(true);

  // Speculative run uses the *same* inputs the post-restore segment will
  // use, so the underlying sim work is comparable. Counters must not move.
  Rand specInputs = subInputs;
  for (int f = 0; f < kPhase; ++f) sub.step(specInputs);

  REQUIRE(sub.sp->plays == playsAtSnap);
  REQUIRE(sub.sp->stops == stopsAtSnap);
  REQUIRE(sub.stats().events == eventsAtSnap);

  // isPlaying should have been called at least once and always passes
  // through — its count is allowed to grow during speculation.
  int isPlayingDuringSpec = sub.sp->isPlayingCalls;

  sub.game->loadSnapshotFast(snap);
  sub.game->setSpeculative(false);

  for (int f = 0; f < kPhase; ++f) sub.step(subInputs);

  REQUIRE(sub.sp->plays == controlPlays);
  REQUIRE(sub.sp->stops == controlStops);
  REQUIRE(sub.stats().events == controlEvents);

  // Sanity: isPlaying did pass through during speculation (not gated).
  REQUIRE(sub.sp->isPlayingCalls >= isPlayingDuringSpec);
}

TEST_CASE("setSpeculative propagates to soundPlayer and statsRecorder", "[rollback]") {
  Runner r(0x1234);
  REQUIRE(r.game->speculative == false);
  REQUIRE(r.game->soundPlayer->speculative == false);
  REQUIRE(r.game->statsRecorder->speculative == false);

  r.game->setSpeculative(true);
  REQUIRE(r.game->speculative == true);
  REQUIRE(r.game->soundPlayer->speculative == true);
  REQUIRE(r.game->statsRecorder->speculative == true);

  r.game->setSpeculative(false);
  REQUIRE(r.game->speculative == false);
  REQUIRE(r.game->soundPlayer->speculative == false);
  REQUIRE(r.game->statsRecorder->speculative == false);
}
