// Round-trip test for multiplayer replay recording. Runs two
// RollbackControllers through the WS→Game transition, captures peer A's
// shadow ReplayWriter into a memory buffer, then plays the recording
// back via ReplayReader and asserts the final wideRollbackChecksum of
// the replayed Game matches the live shadow's.

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <vector>

#include "controller/rollbackController.hpp"
#include "filesystem.hpp"
#include "game.hpp"
#include "gfx/renderer.hpp"
#include "io/stream.hpp"
#include "math.hpp"
#include "mixer/player.hpp"
#include "replay.hpp"
#include "rollback/buffer.hpp"

namespace {

std::pair<std::shared_ptr<Common>, std::shared_ptr<Settings>> MakeEnv() {
  PrecomputeTables();
  auto common = std::make_shared<Common>();
  FsNode tc_root(FsNode("data") / "TC" / "openliero");
  common->load(std::move(tc_root));
  auto settings = std::make_shared<Settings>();
  settings->lives = 10;
  settings->loading_time = 0;
  settings->load_change = true;
  settings->random_level = true;
  settings->game_mode = Settings::kGmKillEmAll;
  settings->select_bot_weapons = 0;
  return {common, settings};
}

std::unique_ptr<RollbackController> MakePeer(std::shared_ptr<Common> common,
                                             std::shared_ptr<Settings> settings, int local_idx,
                                             uint32_t world_seed) {
  auto c = std::make_unique<RollbackController>(common, settings, local_idx);
  c->SetInputDelay(1);
  c->game.rand.Seed(world_seed);
  return c;
}

constexpr uint8_t kBitDown = uint8_t{1} << Worm::kDown;
constexpr uint8_t kBitFire = uint8_t{1} << Worm::kFire;

std::vector<uint8_t> NavigateAndConfirm(int n_down) {
  std::vector<uint8_t> out;
  for (int i = 0; i < n_down; ++i) {
    out.push_back(kBitDown);
    out.push_back(0);
  }
  out.push_back(0);
  out.push_back(kBitFire);
  out.push_back(0);
  return out;
}

}  // namespace

// Common round-trip body parameterised on which peer (localIdx) records.
// Both indices use the same makePeer fixture so the test exercises the
// host (localIdx=0) and client (localIdx=1) paths symmetrically.
static void RunRoundTrip(int recorder_idx) {
  constexpr uint32_t kWorldSeed = 0xDEADBEEF;
  auto [common, settings] = MakeEnv();
  auto a = MakePeer(common, settings, recorder_idx, kWorldSeed);
  auto b = MakePeer(common, settings, recorder_idx ^ 1, kWorldSeed);

  // Capture peer A's replay stream into a vector via VectorWriter, so
  // the test can read it back after the match without touching disk.
  std::vector<uint8_t> replay_bytes;
  a->SetReplayWriterOverride(std::make_unique<io::VectorWriter>(replay_bytes));

  // Direct synchronous batch delivery — same pattern test_rollback_weapsel
  // uses for its zero-jitter case.
  struct Pkt {
    uint32_t base_frame;
    uint8_t count;
    std::array<uint8_t, rollback::kMaxRollback + 1> inputs;
    uint32_t local_frame;
  };
  std::vector<Pkt> a_to_b, b_to_a;
  auto enqueue = [](std::vector<Pkt>& q, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
    Pkt p{};
    p.base_frame = bf;
    p.count = c;
    p.local_frame = lf;
    for (uint8_t i = 0; i < c; ++i) p.inputs[i] = in[i];
    q.push_back(p);
  };
  a->SetInputCallbacks([&](uint8_t, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
    enqueue(a_to_b, bf, c, in, lf);
  });
  b->SetInputCallbacks([&](uint8_t, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
    enqueue(b_to_a, bf, c, in, lf);
  });

  a->Focus();
  b->Focus();
  a->InjectRemoteInput(0, 0);
  b->InjectRemoteInput(0, 0);

  // Run the WS script, then a tail of idle ticks so the WS→Game
  // transition lands. Then run game-phase ticks until peer A's shadow
  // has confirmed a decent number of frames worth of recording.
  auto ws_script = NavigateAndConfirm(6);
  constexpr int kWsTail = 40;
  constexpr int kGameTicks = 120;

  int total_ticks = static_cast<int>(ws_script.size()) + kWsTail + kGameTicks;
  for (int i = 0; i < total_ticks; ++i) {
    uint8_t in = (i < static_cast<int>(ws_script.size())) ? ws_script[i] : 0;
    a->SetLocalControlState(in);
    b->SetLocalControlState(in);
    a->Process();
    b->Process();
    for (auto const& p : a_to_b)
      b->InjectRemoteBatch(p.base_frame, p.count, p.inputs.data(), p.local_frame);
    for (auto const& p : b_to_a)
      a->InjectRemoteBatch(p.base_frame, p.count, p.inputs.data(), p.local_frame);
    a_to_b.clear();
    b_to_a.clear();
  }

  REQUIRE(a->State() == kStateGame);
  REQUIRE(b->State() == kStateGame);
  REQUIRE(a->RollbackCount() == 0);

  // Snapshot the shadow's expected final state BEFORE endMatch so
  // additional fade-out ticks can't advance it past what the recording
  // captured.
  Game* shadow = a->ShadowGameForTest();
  REQUIRE(shadow != nullptr);
  uint32_t shadow_checksum = WideRollbackChecksum(*shadow);
  int shadow_cycles = shadow->cycles;

  // End the match so ReplayWriter's terminator gets flushed and the
  // DeflateWriter inside it gets finalized. Up until this point the
  // DeflateWriter has been buffering uncompressed output; endMatch
  // triggers ~ReplayWriter which triggers ~DeflateWriter which flushes
  // the compressed stream into replayBytes via VectorWriter.
  a->EndMatch();

  REQUIRE(!replay_bytes.empty());

  // Read back. ReplayReader inflates the captured bytes during
  // construction; the Game returned by beginPlayback isn't yet wired
  // into the reader (ReplayController does that, see
  // replayController.cpp:55) so we do it explicitly here.
  ReplayReader rr(std::make_unique<io::MemReader>(replay_bytes));
  auto play_sp = std::make_shared<NullSoundPlayer>();
  std::unique_ptr<Game> playback = rr.BeginPlayback(common, play_sp);
  REQUIRE(playback != nullptr);
  rr.game = playback.get();

  // Step playback until the file's 0x83 terminator.
  Renderer renderer;
  int played_frames = 0;
  while (rr.PlaybackFrame(renderer)) {
    playback->ProcessFrame();
    ++played_frames;
    if (played_frames > 10000) FAIL("playback ran past expected length");
  }

  REQUIRE(played_frames > 0);

  // The playback should land on exactly the same sim state the shadow
  // was in when recording stopped: same cycles count, same wide
  // checksum. Any divergence here means the recorded delta stream
  // doesn't faithfully reproduce the simulation it captured.
  REQUIRE(playback->cycles == shadow_cycles);
  REQUIRE(WideRollbackChecksum(*playback) == shadow_checksum);

  // The recorded level palette must round-trip to the same bytes the
  // shadow saw when it began recording. Regression guard for the
  // multiplayer-replay palette-corruption bug: the fast snapshot
  // doesn't carry origpal, and beginRecord uses cereal which does.
  for (int i = 0; i < 256; ++i) {
    REQUIRE(playback->level.origpal.entries[i].r == shadow->level.origpal.entries[i].r);
    REQUIRE(playback->level.origpal.entries[i].g == shadow->level.origpal.entries[i].g);
    REQUIRE(playback->level.origpal.entries[i].b == shadow->level.origpal.entries[i].b);
  }
  // And the shadow's origpal must match the live game's (the controller
  // that captured the recording). If these diverge, the file is
  // recording a palette the player never saw.
  for (int i = 0; i < 256; ++i) {
    REQUIRE(shadow->level.origpal.entries[i].r == a->game.level.origpal.entries[i].r);
    REQUIRE(shadow->level.origpal.entries[i].g == a->game.level.origpal.entries[i].g);
    REQUIRE(shadow->level.origpal.entries[i].b == a->game.level.origpal.entries[i].b);
  }
}

TEST_CASE("Multiplayer replay round-trip matches live shadow (host)", "[rollback][replay]") {
  RunRoundTrip(0);
}

TEST_CASE("Multiplayer replay round-trip matches live shadow (client)", "[rollback][replay]") {
  RunRoundTrip(1);
}
