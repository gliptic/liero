#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <memory>

#include "game.hpp"
#include "level.hpp"
#include "math.hpp"
#include "mixer/player.hpp"
#include "stateHash.hpp"
#include "viewport.hpp"
#include "weapon.hpp"
#include "worm.hpp"

struct DualGameFixture {
  std::shared_ptr<Common> common;
  std::shared_ptr<Settings> settings;
  std::shared_ptr<SoundPlayer> soundPlayerA;
  std::shared_ptr<SoundPlayer> soundPlayerB;
  std::unique_ptr<Game> gameA;
  std::unique_ptr<Game> gameB;

  DualGameFixture() {
    precomputeTables();

    common = std::make_shared<Common>();
    FsNode tcRoot(FsNode("data") / "TC" / "openliero");
    common->load(std::move(tcRoot));

    settings = std::make_shared<Settings>();
    // Use default settings but ensure deterministic setup
    settings->lives = 10;
    settings->loadingTime = 0;
    settings->randomLevel = true;
    settings->gameMode = Settings::GMKillEmAll;

    soundPlayerA = std::make_shared<NullSoundPlayer>();
    soundPlayerB = std::make_shared<NullSoundPlayer>();

    gameA = std::make_unique<Game>(common, settings, soundPlayerA);
    gameB = std::make_unique<Game>(common, settings, soundPlayerB);

    // Seed both with the same value
    uint32_t seed = 42;
    gameA->rand.seed(seed);
    gameB->rand.seed(seed);

    // Create identical worms for both games
    for (int idx = 0; idx < 2; ++idx) {
      auto wA = std::make_shared<Worm>();
      wA->settings = settings->wormSettings[idx];
      wA->health = wA->settings->health;
      wA->index = idx;
      wA->statsX = idx == 0 ? 0 : 218;

      auto wB = std::make_shared<Worm>();
      wB->settings = settings->wormSettings[idx];
      wB->health = wB->settings->health;
      wB->index = idx;
      wB->statsX = idx == 0 ? 0 : 218;

      gameA->addWorm(wA);
      gameB->addWorm(wB);
    }

    // Add viewports (needed for processFrame's viewport logic)
    gameA->addViewport(new Viewport(Rect(0, 0, 158, 158), 0, 504, 350));
    gameA->addViewport(new Viewport(Rect(160, 0, 318, 158), 1, 504, 350));
    gameB->addViewport(new Viewport(Rect(0, 0, 158, 158), 0, 504, 350));
    gameB->addViewport(new Viewport(Rect(160, 0, 318, 158), 1, 504, 350));

    // Generate levels with the same RNG state
    gameA->level.generateFromSettings(*common, *settings, gameA->rand);
    gameB->level.generateFromSettings(*common, *settings, gameB->rand);

    // Initialize weapons for all worms
    for (auto const& w : gameA->worms) w->initWeapons(*gameA);
    for (auto const& w : gameB->worms) w->initWeapons(*gameB);

    // Start games
    gameA->paused = false;
    gameB->paused = false;
    gameA->startGame();
    gameB->startGame();

    // Reset worms into game
    gameA->resetWorms();
    gameB->resetWorms();
  }

  // Apply identical random inputs to both games using a separate PRNG
  void applyRandomInputs(Rand& inputRng) {
    for (int idx = 0; idx < 2; ++idx) {
      uint32_t input = inputRng() & 0x7f;  // 7 control bits
      gameA->worms[idx]->controlStates.unpack(input);
      gameB->worms[idx]->controlStates.unpack(input);
    }
  }
};

TEST_CASE("Dual simulation produces identical state", "[determinism]") {
  DualGameFixture f;

  Rand inputRng(12345);

  constexpr int NUM_FRAMES = 1000;

  for (int frame = 0; frame < NUM_FRAMES; ++frame) {
    f.applyRandomInputs(inputRng);
    f.gameA->processFrame();
    f.gameB->processFrame();

    uint32_t hashA = hashGameState(*f.gameA);
    uint32_t hashB = hashGameState(*f.gameB);

    INFO("Desync at frame " << frame << ": hashA=0x" << std::hex << hashA
                            << " hashB=0x" << hashB);
    REQUIRE(hashA == hashB);
  }
}

TEST_CASE("Simulation is reproducible across runs", "[determinism]") {
  // Run the same simulation twice from scratch and verify identical results
  uint32_t hash1, hash2;

  for (int run = 0; run < 2; ++run) {
    DualGameFixture f;
    Rand inputRng(99999);

    constexpr int NUM_FRAMES = 500;

    for (int frame = 0; frame < NUM_FRAMES; ++frame) {
      f.applyRandomInputs(inputRng);
      f.gameA->processFrame();
    }

    uint32_t h = hashGameState(*f.gameA);
    if (run == 0)
      hash1 = h;
    else
      hash2 = h;
  }

  REQUIRE(hash1 == hash2);
}

TEST_CASE("Same inputs produce same state regardless of construction order",
          "[determinism]") {
  // Construct games in different order but with same seed — should be identical
  auto common = std::make_shared<Common>();
  FsNode tcRoot(FsNode("data") / "TC" / "openliero");
  common->load(std::move(tcRoot));

  precomputeTables();

  auto settings = std::make_shared<Settings>();
  settings->randomLevel = true;

  auto sp = std::make_shared<NullSoundPlayer>();

  // Game constructed first
  Game game1(common, settings, sp);
  game1.rand.seed(777);

  // Game constructed after some unrelated work
  volatile int dummy = 0;
  for (int i = 0; i < 1000; ++i) dummy += i;
  (void)dummy;

  Game game2(common, settings, std::make_shared<NullSoundPlayer>());
  game2.rand.seed(777);

  // Same worm setup
  for (int idx = 0; idx < 2; ++idx) {
    auto w1 = std::make_shared<Worm>();
    w1->settings = settings->wormSettings[idx];
    w1->health = w1->settings->health;
    w1->index = idx;
    game1.addWorm(w1);

    auto w2 = std::make_shared<Worm>();
    w2->settings = settings->wormSettings[idx];
    w2->health = w2->settings->health;
    w2->index = idx;
    game2.addWorm(w2);
  }

  game1.addViewport(new Viewport(Rect(0, 0, 158, 158), 0, 504, 350));
  game1.addViewport(new Viewport(Rect(160, 0, 318, 158), 1, 504, 350));
  game2.addViewport(new Viewport(Rect(0, 0, 158, 158), 0, 504, 350));
  game2.addViewport(new Viewport(Rect(160, 0, 318, 158), 1, 504, 350));

  game1.level.generateFromSettings(*common, *settings, game1.rand);
  game2.level.generateFromSettings(*common, *settings, game2.rand);

  for (auto const& w : game1.worms) w->initWeapons(game1);
  for (auto const& w : game2.worms) w->initWeapons(game2);

  game1.paused = false;
  game2.paused = false;
  game1.startGame();
  game2.startGame();
  game1.resetWorms();
  game2.resetWorms();

  Rand inputRng1(55555);
  Rand inputRng2(55555);

  for (int frame = 0; frame < 300; ++frame) {
    for (int idx = 0; idx < 2; ++idx) {
      uint32_t input1 = inputRng1() & 0x7f;
      uint32_t input2 = inputRng2() & 0x7f;
      game1.worms[idx]->controlStates.unpack(input1);
      game2.worms[idx]->controlStates.unpack(input2);
    }
    game1.processFrame();
    game2.processFrame();
  }

  REQUIRE(hashGameState(game1) == hashGameState(game2));
}

TEST_CASE("Death and respawn determinism fuzz", "[determinism][death]") {
  // Stress-test the death/respawn path with aggressive inputs over many frames.
  // Uses low health so deaths occur frequently, and biases inputs toward combat
  // (fire held + movement) to maximize projectile/death interactions.
  //
  // This targets the known desync risk in beginRespawn() where the RNG-based
  // position search depends on level pixel state.

  precomputeTables();

  auto common = std::make_shared<Common>();
  FsNode tcRoot(FsNode("data") / "TC" / "openliero");
  common->load(std::move(tcRoot));

  auto settings = std::make_shared<Settings>();
  settings->lives = 50;       // Many lives = many death/respawn cycles
  settings->loadingTime = 0;  // Fast weapon reload
  settings->randomLevel = true;
  settings->gameMode = Settings::GMKillEmAll;
  settings->blood = 100;

  // Use multiple seeds to cover different level layouts
  uint32_t seeds[] = { 42, 1337, 99999, 0xDEAD, 0xBEEF };

  for (uint32_t seed : seeds) {
    auto spA = std::make_shared<NullSoundPlayer>();
    auto spB = std::make_shared<NullSoundPlayer>();

    Game gameA(common, settings, spA);
    Game gameB(common, settings, spB);

    gameA.rand.seed(seed);
    gameB.rand.seed(seed);

    for (int idx = 0; idx < 2; ++idx) {
      auto wA = std::make_shared<Worm>();
      wA->settings = settings->wormSettings[idx];
      wA->health = 25;  // Low health for quick deaths
      wA->index = idx;
      wA->statsX = idx == 0 ? 0 : 218;
      gameA.addWorm(wA);

      auto wB = std::make_shared<Worm>();
      wB->settings = settings->wormSettings[idx];
      wB->health = 25;
      wB->index = idx;
      wB->statsX = idx == 0 ? 0 : 218;
      gameB.addWorm(wB);
    }

    gameA.addViewport(new Viewport(Rect(0, 0, 158, 158), 0, 504, 350));
    gameA.addViewport(new Viewport(Rect(160, 0, 318, 158), 1, 504, 350));
    gameB.addViewport(new Viewport(Rect(0, 0, 158, 158), 0, 504, 350));
    gameB.addViewport(new Viewport(Rect(160, 0, 318, 158), 1, 504, 350));

    gameA.level.generateFromSettings(*common, *settings, gameA.rand);
    gameB.level.generateFromSettings(*common, *settings, gameB.rand);

    for (auto const& w : gameA.worms) w->initWeapons(gameA);
    for (auto const& w : gameB.worms) w->initWeapons(gameB);

    gameA.paused = false;
    gameB.paused = false;
    gameA.startGame();
    gameB.startGame();
    gameA.resetWorms();
    gameB.resetWorms();

    Rand inputRng(seed * 2654435761u + 1);

    constexpr int NUM_FRAMES = 5000;
    int deathCount = 0;

    for (int frame = 0; frame < NUM_FRAMES; ++frame) {
      for (int idx = 0; idx < 2; ++idx) {
        uint32_t input = inputRng() & 0x7f;
        // Bias toward combat: 60% chance fire is held
        if ((inputRng() % 10) < 6)
          input |= (1 << 4);  // Fire bit
        // 40% chance of movement toward opponent
        if ((inputRng() % 10) < 4)
          input |= (1 << (idx == 0 ? 1 : 0));  // Left/Right toward other

        gameA.worms[idx]->controlStates.unpack(input);
        gameB.worms[idx]->controlStates.unpack(input);
      }

      gameA.processFrame();
      gameB.processFrame();

      // Track deaths for info output — killedTimer is set to 150 on death,
      // then decremented each frame, so 149 means "just died this frame"
      for (auto const& w : gameA.worms) {
        if (!w->visible && w->killedTimer == Worm::KilledTimerInitial - 1)
          ++deathCount;
      }

      // Check state every frame
      uint32_t hashA = hashGameState(gameA);
      uint32_t hashB = hashGameState(gameB);

      if (hashA != hashB) {
        // Identify which component diverged
        uint32_t rngA = gameA.rand.last;
        uint32_t rngB = gameB.rand.last;
        bool rngMatch = (rngA == rngB);

        bool wormsMatch = true;
        for (size_t i = 0; i < gameA.worms.size(); ++i) {
          if (gameA.worms[i]->pos.x != gameB.worms[i]->pos.x ||
              gameA.worms[i]->pos.y != gameB.worms[i]->pos.y ||
              gameA.worms[i]->visible != gameB.worms[i]->visible ||
              gameA.worms[i]->health != gameB.worms[i]->health ||
              gameA.worms[i]->killedTimer != gameB.worms[i]->killedTimer) {
            wormsMatch = false;
            break;
          }
        }

        // Check level pixels
        bool levelMatch = true;
        for (int i = 0; i < gameA.level.width * gameA.level.height; ++i) {
          if (gameA.level.data[i] != gameB.level.data[i]) {
            levelMatch = false;
            break;
          }
        }

        // Check object counts
        int nobjectsA = 0, nobjectsB = 0;
        { auto r = gameA.nobjects.all(); NObject* n; while ((n = r.next())) ++nobjectsA; }
        { auto r = gameB.nobjects.all(); NObject* n; while ((n = r.next())) ++nobjectsB; }

        int bobjectsA = 0, bobjectsB = 0;
        { auto br = gameA.bobjects.begin(); for (; br != gameA.bobjects.end(); ++br) ++bobjectsA; }
        { auto br = gameB.bobjects.begin(); for (; br != gameB.bobjects.end(); ++br) ++bobjectsB; }

        // Deep compare BObjects
        bool bobjectsMatch = true;
        {
          auto brA = gameA.bobjects.begin();
          auto brB = gameB.bobjects.begin();
          int idx = 0;
          for (; brA != gameA.bobjects.end() && brB != gameB.bobjects.end(); ++brA, ++brB, ++idx) {
            if (brA->pos.x != brB->pos.x || brA->pos.y != brB->pos.y) {
              INFO("  BObject[" << idx << "] pos differs: A=(" << brA->pos.x << "," << brA->pos.y
                   << ") B=(" << brB->pos.x << "," << brB->pos.y << ")");
              bobjectsMatch = false;
              break;
            }
          }
        }

        // Deep compare NObjects
        bool nobjectsMatch = true;
        {
          auto rA = gameA.nobjects.all(); auto rB = gameB.nobjects.all();
          NObject *nA, *nB; int idx = 0;
          while ((nA = rA.next()) && (nB = rB.next())) {
            if (nA->pos.x != nB->pos.x || nA->pos.y != nB->pos.y ||
                nA->vel.x != nB->vel.x || nA->vel.y != nB->vel.y) {
              INFO("  NObject[" << idx << "] differs: A pos=(" << nA->pos.x << "," << nA->pos.y
                   << ") vel=(" << nA->vel.x << "," << nA->vel.y
                   << ") B pos=(" << nB->pos.x << "," << nB->pos.y
                   << ") vel=(" << nB->vel.x << "," << nB->vel.y << ")");
              nobjectsMatch = false;
              break;
            }
            ++idx;
          }
        }

        // Deep compare WObjects
        bool wobjectsMatch = true;
        int wobjectsA = 0, wobjectsB = 0;
        {
          auto rA = gameA.wobjects.all(); auto rB = gameB.wobjects.all();
          WObject *wA, *wB; int idx = 0;
          while ((wA = rA.next())) { ++wobjectsA; (void)wA; }
          while ((wB = rB.next())) { ++wobjectsB; (void)wB; }
          rA = gameA.wobjects.all(); rB = gameB.wobjects.all();
          while ((wA = rA.next()) && (wB = rB.next())) {
            if (wA->pos.x != wB->pos.x || wA->pos.y != wB->pos.y ||
                wA->vel.x != wB->vel.x || wA->vel.y != wB->vel.y ||
                wA->curFrame != wB->curFrame || wA->timeLeft != wB->timeLeft) {
              INFO("  WObject[" << idx << "] differs: A pos=(" << wA->pos.x << "," << wA->pos.y
                   << ") tl=" << wA->timeLeft
                   << " B pos=(" << wB->pos.x << "," << wB->pos.y
                   << ") tl=" << wB->timeLeft);
              wobjectsMatch = false;
              break;
            }
            ++idx;
          }
        }

        // Deep compare SObjects
        bool sobjectsMatch = true;
        int sobjectsA = 0, sobjectsB = 0;
        {
          auto rA = gameA.sobjects.all(); auto rB = gameB.sobjects.all();
          SObject *sA, *sB; int idx = 0;
          while ((sA = rA.next())) { ++sobjectsA; }
          while ((sB = rB.next())) { ++sobjectsB; }
          rA = gameA.sobjects.all(); rB = gameB.sobjects.all();
          while ((sA = rA.next()) && (sB = rB.next())) {
            if (sA->id != sB->id || sA->curFrame != sB->curFrame) {
              INFO("  SObject[" << idx << "] differs: A id=" << sA->id << " frame=" << sA->curFrame
                   << " B id=" << sB->id << " frame=" << sB->curFrame);
              sobjectsMatch = false;
              break;
            }
            ++idx;
          }
        }

        // Deep compare Bonuses
        bool bonusesMatch = true;
        int bonusesA = 0, bonusesB = 0;
        {
          auto rA = gameA.bonuses.all(); auto rB = gameB.bonuses.all();
          Bonus *bA, *bB; int idx = 0;
          while ((bA = rA.next())) { ++bonusesA; }
          while ((bB = rB.next())) { ++bonusesB; }
          rA = gameA.bonuses.all(); rB = gameB.bonuses.all();
          while ((bA = rA.next()) && (bB = rB.next())) {
            if (bA->x != bB->x || bA->y != bB->y || bA->timer != bB->timer || bA->weapon != bB->weapon) {
              INFO("  Bonus[" << idx << "] differs");
              bonusesMatch = false;
              break;
            }
            ++idx;
          }
        }

        // Deep compare worm weapons
        bool weaponsMatch = true;
        for (size_t wi = 0; wi < gameA.worms.size(); ++wi) {
          auto const& wA = gameA.worms[wi]; auto const& wB = gameB.worms[wi];
          for (int i = 0; i < NUM_WEAPONS; ++i) {
            if (wA->weapons[i].ammo != wB->weapons[i].ammo ||
                wA->weapons[i].delayLeft != wB->weapons[i].delayLeft ||
                wA->weapons[i].loadingLeft != wB->weapons[i].loadingLeft) {
              INFO("  Worm[" << wi << "].weapons[" << i << "] differs: A ammo=" << wA->weapons[i].ammo
                   << " delay=" << wA->weapons[i].delayLeft << " loading=" << wA->weapons[i].loadingLeft
                   << " B ammo=" << wB->weapons[i].ammo
                   << " delay=" << wB->weapons[i].delayLeft << " loading=" << wB->weapons[i].loadingLeft);
              weaponsMatch = false;
            }
          }
        }

        INFO("Desync at frame " << frame << " (seed=" << seed
             << ", deaths=" << deathCount << ")"
             << "\n  RNG match=" << rngMatch
             << " Worms match=" << wormsMatch
             << " Level match=" << levelMatch
             << "\n  NObjects: A=" << nobjectsA << " B=" << nobjectsB
             << " match=" << nobjectsMatch
             << "\n  BObjects: A=" << bobjectsA << " B=" << bobjectsB
             << " match=" << bobjectsMatch
             << "\n  WObjects: A=" << wobjectsA << " B=" << wobjectsB
             << " match=" << wobjectsMatch
             << "\n  SObjects: A=" << sobjectsA << " B=" << sobjectsB
             << " match=" << sobjectsMatch
             << "\n  Bonuses: A=" << bonusesA << " B=" << bonusesB
             << " match=" << bonusesMatch
             << "\n  Weapons match=" << weaponsMatch
             << "\n  hashA=0x" << std::hex << hashA
             << " hashB=0x" << hashB);
        REQUIRE(hashA == hashB);
      }

      // Stop if game is over
      if (gameA.isGameOver())
        break;
    }

    INFO("Seed " << seed << " completed: " << deathCount << " deaths observed");
    REQUIRE(deathCount > 0);  // Sanity check: we actually tested deaths
  }
}
