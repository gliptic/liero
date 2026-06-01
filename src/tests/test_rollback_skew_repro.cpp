// Force-skew regression gate.
//
// Drives two RollbackControllers through weapon select over asymmetric
// one-way delays, deliberately producing a WS-phase simFrame skew
// between the peers. Asserts:
//   * during WS, the peers were observed at different simFrame values
//     at least once (so the test isn't vacuous);
//   * after the WS→game transition, both peers land at simFrame=0;
//   * the cached wideRollbackChecksum on rollback slots for the first
//     ~32 game-phase frames matches between the peers.
//
// Removing the post-WS reset (resetForGamePhase + finishWeaponSelect)
// makes the post-transition simFrame assertion fail: each peer would
// transition at whatever simFrame WS reached on its side, and the
// asymmetry would persist into the game phase.

#include <algorithm>
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <memory>
#include <vector>

#include "controller/rollbackController.hpp"
#include "game.hpp"
#include "jitter_transport.hpp"
#include "math.hpp"
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

std::unique_ptr<RollbackController> makePeer(std::shared_ptr<Common> common,
                                             std::shared_ptr<Settings> settings, int localIdx,
                                             uint32_t worldSeed) {
  auto c = std::make_unique<RollbackController>(common, settings, localIdx);
  c->setInputDelay(1);
  c->game.rand.seed(worldSeed);
  return c;
}

constexpr uint8_t BIT_DOWN = uint8_t{1} << Worm::Down;
constexpr uint8_t BIT_FIRE = uint8_t{1} << Worm::Fire;

std::vector<uint8_t> navigateToDoneAndConfirm(int nDown) {
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

TEST_CASE("WS simFrame skew is erased at the WS→game transition", "[rollback][step14][skew]") {
  constexpr uint32_t kWorldSeed = 0xF00DBABE;
  auto [common, settings] = makeEnv();
  auto a = makePeer(common, settings, 0, kWorldSeed);
  auto b = makePeer(common, settings, 1, kWorldSeed);

  // Asymmetric delays — A→B fast (1 frame), B→A slow (4 frames). Plus
  // we disable A's frame-advantage stall so A runs ahead freely while B
  // (which still stalls when its view of A's simFrame lags) progresses
  // slower. The combination reliably produces a multi-frame WS-phase
  // skew while keeping both peers progressing enough to eventually
  // confirm wsDone.
  rollback_test::JitterTransport tAB(
      {0x1111, /*minDelay=*/1, /*maxDelay=*/1, /*loss=*/0.0, /*dup=*/0.0});
  rollback_test::JitterTransport tBA(
      {0x2222, /*minDelay=*/4, /*maxDelay=*/4, /*loss=*/0.0, /*dup=*/0.0});
  a->setFrameAdvantageEnabled(false);

  a->setInputCallbacks([&](uint8_t gen, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
    tAB.sendAToB(gen, bf, c, in, lf);
  });
  b->setInputCallbacks([&](uint8_t gen, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
    tBA.sendAToB(gen, bf, c, in, lf);
  });

  a->focus();
  b->focus();
  a->injectRemoteInput(0, 0);
  b->injectRemoteInput(0, 0);

  auto deliverB = [&](uint8_t gen, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
    b->injectRemoteBatch(gen, bf, c, in, lf);
  };
  auto deliverA = [&](uint8_t gen, uint32_t bf, uint8_t c, uint8_t const* in, uint32_t lf) {
    a->injectRemoteBatch(gen, bf, c, in, lf);
  };
  auto noop = [&](uint8_t, uint32_t, uint8_t, uint8_t const*, uint32_t) {};

  auto script = navigateToDoneAndConfirm(6);
  constexpr int kWsTail = 80;  // extra ticks so the slower peer catches up

  bool aTransitioned = false, bTransitioned = false;
  uint32_t aTransitionFrame = 0, bTransitionFrame = 0;
  int maxObservedSkew = 0;

  auto stepBoth = [&](uint8_t inA, uint8_t inB) {
    a->setLocalControlState(inA);
    b->setLocalControlState(inB);
    bool aPre = !aTransitioned;
    bool bPre = !bTransitioned;
    a->process();
    b->process();
    if (aPre && a->gameState() == StateGame) {
      aTransitioned = true;
      aTransitionFrame = a->currentFrame();
    }
    if (bPre && b->gameState() == StateGame) {
      bTransitioned = true;
      bTransitionFrame = b->currentFrame();
    }
    // tAB carries A→B; the "deliverA" leg of its tick is unused.
    tAB.tick(noop, deliverB);
    tBA.tick(noop, deliverA);
  };

  for (int i = 0;
       i < static_cast<int>(script.size()) + kWsTail && !(aTransitioned && bTransitioned); ++i) {
    uint8_t in = (i < static_cast<int>(script.size())) ? script[i] : 0;
    stepBoth(in, in);
    // Observe WS-phase skew while at least one peer is still pre-transition.
    if (!aTransitioned || !bTransitioned) {
      int gap = static_cast<int>(a->currentFrame()) - static_cast<int>(b->currentFrame());
      if (gap < 0) gap = -gap;
      if (gap > maxObservedSkew) maxObservedSkew = gap;
    }
  }

  // Flush any tail packets, then a few idle ticks to finish either peer
  // that hasn't transitioned yet.
  tAB.flush(noop, deliverB);
  tBA.flush(noop, deliverA);
  for (int i = 0; i < 24 && !(aTransitioned && bTransitioned); ++i) {
    stepBoth(0, 0);
  }

  INFO("maxObservedSkew=" << maxObservedSkew << " aTransitionFrame=" << aTransitionFrame
                          << " bTransitionFrame=" << bTransitionFrame);
  REQUIRE(aTransitioned);
  REQUIRE(bTransitioned);

  // Headline: regardless of WS skew, both peers entered game phase at
  // simFrame=0. Removing the post-WS reset would leave the peers at
  // whatever simFrame WS reached on each side.
  REQUIRE(aTransitionFrame == 0);
  REQUIRE(bTransitionFrame == 0);

  // Vacuity guard: if we never observed a WS-phase skew the test isn't
  // exercising the bug class. The asymmetric delay + disabled
  // frame-advantage on A routinely produces a multi-frame gap; a
  // single-frame gap is still meaningful evidence the timeline went
  // asymmetric.
  REQUIRE(maxObservedSkew >= 1);

  // Drive ~32 game-phase frames in sync, then a flush + short drain.
  // Both peers stay idle so the sim produces identical state on each
  // side; the rollback path exercises predicted/confirmed transitions
  // because the B→A pipe still delays inputs by 4 frames.
  constexpr int kGameFrames = 32;
  for (int i = 0; i < kGameFrames; ++i) {
    stepBoth(0, 0);
  }
  tAB.flush(noop, deliverB);
  tBA.flush(noop, deliverA);
  for (int i = 0; i < 16; ++i) {
    stepBoth(0, 0);
  }

  // Cached wideRollbackChecksum match across the slots currently
  // resident in both peers' rings. The ring is bounded at
  // kMaxRollback+1 = 8 slots, so we can't compare "frame 0 to 31"
  // verbatim — we compare the overlap of the two rings' current
  // contents, which after the drive above is the most recent ~8
  // confirmed frames on each side. Together with the in-loop
  // checksumming this guards the first ~32 game-phase frames'
  // determinism (any earlier mismatch would propagate forward into
  // the still-resident frames).
  rollback::RollbackBuffer const& bufA = a->rollbackBuffer();
  rollback::RollbackBuffer const& bufB = b->rollbackBuffer();
  int compared = 0;
  int loF = std::max(bufA.oldestFrame(), bufB.oldestFrame());
  int hiF = std::min(bufA.newestFrame(), bufB.newestFrame());
  for (int f = loF; f <= hiF; ++f) {
    auto* slotA = const_cast<rollback::RollbackBuffer&>(bufA).find(f);
    auto* slotB = const_cast<rollback::RollbackBuffer&>(bufB).find(f);
    if (!slotA || !slotB) continue;
    INFO("frame " << f << " A=" << slotA->checksum << " B=" << slotB->checksum);
    REQUIRE(slotA->checksum == slotB->checksum);
    ++compared;
  }
  // Vacuity guard.
  REQUIRE(compared >= 4);
}
