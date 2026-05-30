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

std::pair<std::shared_ptr<Common>, std::shared_ptr<Settings>> makeEnv() {
  precomputeTables();
  auto common = std::make_shared<Common>();
  FsNode tcRoot(FsNode("data") / "TC" / "openliero");
  common->load(std::move(tcRoot));
  auto settings = std::make_shared<Settings>();
  settings->lives = 10;
  settings->loadingTime = 0;
  settings->loadChange = true;
  settings->randomLevel = true;
  settings->gameMode = Settings::GMKillEmAll;
  settings->selectBotWeapons = 0;
  return {common, settings};
}

std::unique_ptr<RollbackController> makePeer(
    std::shared_ptr<Common> common,
    std::shared_ptr<Settings> settings,
    int localIdx,
    uint32_t worldSeed) {
  auto c = std::make_unique<RollbackController>(common, settings, localIdx);
  c->setInputDelay(1);
  c->game.rand.seed(worldSeed);
  return c;
}

constexpr uint8_t BIT_DOWN = uint8_t{1} << Worm::Down;
constexpr uint8_t BIT_FIRE = uint8_t{1} << Worm::Fire;

std::vector<uint8_t> navigateAndConfirm(int nDown) {
  std::vector<uint8_t> out;
  for (int i = 0; i < nDown; ++i) {
    out.push_back(BIT_DOWN);
    out.push_back(0);
  }
  out.push_back(0);
  out.push_back(BIT_FIRE);
  out.push_back(0);
  return out;
}

}  // namespace

// Common round-trip body parameterised on which peer (localIdx) records.
// Both indices use the same makePeer fixture so the test exercises the
// host (localIdx=0) and client (localIdx=1) paths symmetrically.
static void runRoundTrip(int recorderIdx) {
  constexpr uint32_t kWorldSeed = 0xDEADBEEF;
  auto [common, settings] = makeEnv();
  auto a = makePeer(common, settings, recorderIdx, kWorldSeed);
  auto b = makePeer(common, settings, recorderIdx ^ 1, kWorldSeed);

  // Capture peer A's replay stream into a vector via VectorWriter, so
  // the test can read it back after the match without touching disk.
  std::vector<uint8_t> replayBytes;
  a->setReplayWriterOverride(std::make_unique<io::VectorWriter>(replayBytes));

  // Direct synchronous batch delivery — same pattern test_rollback_weapsel
  // uses for its zero-jitter case.
  struct Pkt { uint32_t baseFrame; uint8_t count;
               std::array<uint8_t, rollback::kMaxRollback + 1> inputs;
               uint32_t localFrame; };
  std::vector<Pkt> aToB, bToA;
  auto enqueue = [](std::vector<Pkt>& q, uint32_t bf, uint8_t c,
                    uint8_t const* in, uint32_t lf) {
    Pkt p{}; p.baseFrame = bf; p.count = c; p.localFrame = lf;
    for (uint8_t i = 0; i < c; ++i) p.inputs[i] = in[i];
    q.push_back(p);
  };
  a->setInputCallbacks(
      [&](uint8_t, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
        enqueue(aToB, bf, c, in, lf);
      });
  b->setInputCallbacks(
      [&](uint8_t, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
        enqueue(bToA, bf, c, in, lf);
      });

  a->focus();
  b->focus();
  a->injectRemoteInput(0, 0);
  b->injectRemoteInput(0, 0);

  // Run the WS script, then a tail of idle ticks so the WS→Game
  // transition lands. Then run game-phase ticks until peer A's shadow
  // has confirmed a decent number of frames worth of recording.
  auto wsScript = navigateAndConfirm(6);
  constexpr int kWsTail = 40;
  constexpr int kGameTicks = 120;

  int totalTicks = static_cast<int>(wsScript.size()) + kWsTail + kGameTicks;
  for (int i = 0; i < totalTicks; ++i) {
    uint8_t in = (i < static_cast<int>(wsScript.size())) ? wsScript[i] : 0;
    a->setLocalControlState(in);
    b->setLocalControlState(in);
    a->process();
    b->process();
    for (auto const& p : aToB)
      b->injectRemoteBatch(p.baseFrame, p.count, p.inputs.data(), p.localFrame);
    for (auto const& p : bToA)
      a->injectRemoteBatch(p.baseFrame, p.count, p.inputs.data(), p.localFrame);
    aToB.clear();
    bToA.clear();
  }

  REQUIRE(a->gameState() == StateGame);
  REQUIRE(b->gameState() == StateGame);
  REQUIRE(a->rollbackCount() == 0);

  // Snapshot the shadow's expected final state BEFORE endMatch so
  // additional fade-out ticks can't advance it past what the recording
  // captured.
  Game* shadow = a->shadowGameForTest();
  REQUIRE(shadow != nullptr);
  uint32_t shadowChecksum = wideRollbackChecksum(*shadow);
  int shadowCycles = shadow->cycles;

  // End the match so ReplayWriter's terminator gets flushed and the
  // DeflateWriter inside it gets finalized. Up until this point the
  // DeflateWriter has been buffering uncompressed output; endMatch
  // triggers ~ReplayWriter which triggers ~DeflateWriter which flushes
  // the compressed stream into replayBytes via VectorWriter.
  a->endMatch();

  REQUIRE(!replayBytes.empty());

  // Read back. ReplayReader inflates the captured bytes during
  // construction; the Game returned by beginPlayback isn't yet wired
  // into the reader (ReplayController does that, see
  // replayController.cpp:55) so we do it explicitly here.
  ReplayReader rr(std::make_unique<io::MemReader>(replayBytes));
  auto playSp = std::make_shared<NullSoundPlayer>();
  std::unique_ptr<Game> playback = rr.beginPlayback(common, playSp);
  REQUIRE(playback != nullptr);
  rr.game = playback.get();

  // Step playback until the file's 0x83 terminator.
  Renderer renderer;
  int playedFrames = 0;
  while (rr.playbackFrame(renderer)) {
    playback->processFrame();
    ++playedFrames;
    if (playedFrames > 10000) FAIL("playback ran past expected length");
  }

  REQUIRE(playedFrames > 0);

  // The playback should land on exactly the same sim state the shadow
  // was in when recording stopped: same cycles count, same wide
  // checksum. Any divergence here means the recorded delta stream
  // doesn't faithfully reproduce the simulation it captured.
  REQUIRE(playback->cycles == shadowCycles);
  REQUIRE(wideRollbackChecksum(*playback) == shadowChecksum);

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

TEST_CASE("Multiplayer replay round-trip matches live shadow (host)",
          "[rollback][replay]") {
  runRoundTrip(0);
}

TEST_CASE("Multiplayer replay round-trip matches live shadow (client)",
          "[rollback][replay]") {
  runRoundTrip(1);
}
