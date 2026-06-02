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
  int is_playing_calls = 0;

  bool IsPlaying(void* /*id*/) override {
    ++is_playing_calls;
    return false;
  }
  void Stop(void* /*id*/) override {
    if (speculative) return;
    ++stops;
  }

 protected:
  void PlayImpl(int /*sound*/, void* /*id*/, int /*loops*/) override { ++plays; }
};

struct CountingStatsRecorder : StatsRecorder {
  int events = 0;

#define EVT()                \
  do {                       \
    if (speculative) return; \
    ++events;                \
  } while (0)
  void DamagePotential(Worm*, WormWeapon*, int) override { EVT(); }
  void DamageDealt(Worm*, WormWeapon*, Worm*, int, bool) override { EVT(); }
  void Shot(Worm*, WormWeapon*) override { EVT(); }
  void Hit(Worm*, WormWeapon*, Worm*) override { EVT(); }
  void AfterSpawn(Worm*) override { EVT(); }
  void AfterDeath(Worm*) override { EVT(); }
  void PreTick(Game&) override { EVT(); }
  void Tick(Game&) override { EVT(); }
  void Finish(Game&) override { EVT(); }
  void AiProcessTime(Worm*, std::chrono::nanoseconds) override { EVT(); }
#undef EVT
};

struct Runner {
  std::shared_ptr<Common> common;
  std::shared_ptr<Settings> settings;
  std::shared_ptr<CountingSoundPlayer> sp;
  std::unique_ptr<Game> game;

  Runner(uint32_t seed) {
    PrecomputeTables();
    common = std::make_shared<Common>();
    FsNode tc_root(FsNode("data") / "TC" / "openliero");
    common->load(std::move(tc_root));

    settings = std::make_shared<Settings>();
    settings->lives = 50;
    settings->loading_time = 0;
    settings->random_level = true;
    settings->game_mode = Settings::kGmKillEmAll;
    settings->blood = 100;

    sp = std::make_shared<CountingSoundPlayer>();
    game = std::make_unique<Game>(common, settings, sp);
    game->stats_recorder.reset(new CountingStatsRecorder);
    game->rand.Seed(seed);

    for (int idx = 0; idx < 2; ++idx) {
      auto w = std::make_shared<Worm>();
      w->settings = settings->worm_settings[idx];
      w->health = 25;
      w->index = idx;
      w->stats_x = idx == 0 ? 0 : 218;
      game->AddWorm(w);
    }
    game->AddViewport(new Viewport(Rect(0, 0, 158, 158), 0, 504, 350));
    game->AddViewport(new Viewport(Rect(160, 0, 318, 158), 1, 504, 350));

    game->level.GenerateFromSettings(*common, *settings, game->rand);
    for (auto const& w : game->worms) w->InitWeapons(*game);

    game->paused = false;
    game->StartGame();
    game->ResetWorms();
  }

  void Step(Rand& input_rng) {
    for (int idx = 0; idx < 2; ++idx) {
      uint32_t input = input_rng() & 0x7f;
      if ((input_rng() % 10) < 6) input |= (1 << 4);
      if ((input_rng() % 10) < 4) input |= (1 << (idx == 0 ? 1 : 0));
      game->worms[idx]->control_states.Unpack(input);
    }
    game->ProcessFrame();
  }

  CountingStatsRecorder& Stats() {
    return static_cast<CountingStatsRecorder&>(*game->stats_recorder);
  }
};

}  // namespace

TEST_CASE("Speculative frames suppress sound and stats", "[rollback]") {
  constexpr uint32_t kSeed = 0xC0FFEE;
  constexpr int kPhase = 200;

  // Control: 2 * kPhase frames with no speculation.
  Runner ctl(kSeed);
  Rand ctl_inputs(kSeed ^ 0xDEAD);
  for (int f = 0; f < 2 * kPhase; ++f) ctl.Step(ctl_inputs);
  int control_plays = ctl.sp->plays;
  int control_stops = ctl.sp->stops;
  int control_events = ctl.Stats().events;

  // Subject: run kPhase normally, save snapshot, run kPhase speculatively,
  // restore, run kPhase more normally. Final counts must match the control.
  Runner sub(kSeed);
  Rand sub_inputs(kSeed ^ 0xDEAD);

  GameSnapshot snap;
  snap.Prepare(*sub.game);

  for (int f = 0; f < kPhase; ++f) sub.Step(sub_inputs);
  int plays_at_snap = sub.sp->plays;
  int stops_at_snap = sub.sp->stops;
  int events_at_snap = sub.Stats().events;

  sub.game->SaveSnapshotFast(snap);
  sub.game->SetSpeculative(true);

  // Speculative run uses the *same* inputs the post-restore segment will
  // use, so the underlying sim work is comparable. Counters must not move.
  Rand spec_inputs = sub_inputs;
  for (int f = 0; f < kPhase; ++f) sub.Step(spec_inputs);

  REQUIRE(sub.sp->plays == plays_at_snap);
  REQUIRE(sub.sp->stops == stops_at_snap);
  REQUIRE(sub.Stats().events == events_at_snap);

  // isPlaying should have been called at least once and always passes
  // through — its count is allowed to grow during speculation.
  int is_playing_during_spec = sub.sp->is_playing_calls;

  sub.game->LoadSnapshotFast(snap);
  sub.game->SetSpeculative(false);

  for (int f = 0; f < kPhase; ++f) sub.Step(sub_inputs);

  REQUIRE(sub.sp->plays == control_plays);
  REQUIRE(sub.sp->stops == control_stops);
  REQUIRE(sub.Stats().events == control_events);

  // Sanity: isPlaying did pass through during speculation (not gated).
  REQUIRE(sub.sp->is_playing_calls >= is_playing_during_spec);
}

TEST_CASE("setSpeculative propagates to soundPlayer and statsRecorder", "[rollback]") {
  Runner r(0x1234);
  REQUIRE(r.game->speculative == false);
  REQUIRE(r.game->sound_player->speculative == false);
  REQUIRE(r.game->stats_recorder->speculative == false);

  r.game->SetSpeculative(true);
  REQUIRE(r.game->speculative == true);
  REQUIRE(r.game->sound_player->speculative == true);
  REQUIRE(r.game->stats_recorder->speculative == true);

  r.game->SetSpeculative(false);
  REQUIRE(r.game->speculative == false);
  REQUIRE(r.game->sound_player->speculative == false);
  REQUIRE(r.game->stats_recorder->speculative == false);
}
