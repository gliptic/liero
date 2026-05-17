#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <memory>

#include "game.hpp"
#include "level.hpp"
#include "math.hpp"
#include "mixer/player.hpp"
#include "viewport.hpp"
#include "weapon.hpp"
#include "worm.hpp"

// Hash the simulation-relevant state of a Game into a single 32-bit value.
// This intentionally skips rendering-only state (viewport banners, etc.)
// to focus on what matters for lockstep determinism.
static uint32_t hashGameState(Game& game) {
  uint32_t h = 1;

  // RNG state
  h = h * 31 + game.rand.x;
  h = h * 31 + game.rand.c;

  // Frame counter
  h = h * 31 + static_cast<uint32_t>(game.cycles);

  // Level pixel data (the terrain can be destroyed)
  for (int i = 0; i < game.level.width * game.level.height; ++i) {
    h = h * 33 ^ game.level.data[i];
  }

  // Worm state
  for (auto* w : game.worms) {
    h = h * 31 + static_cast<uint32_t>(w->pos.x);
    h = h * 31 + static_cast<uint32_t>(w->pos.y);
    h = h * 31 + static_cast<uint32_t>(w->vel.x);
    h = h * 31 + static_cast<uint32_t>(w->vel.y);
    h = h * 31 + static_cast<uint32_t>(w->aimingAngle);
    h = h * 31 + static_cast<uint32_t>(w->health);
    h = h * 31 + static_cast<uint32_t>(w->lives);
    h = h * 31 + static_cast<uint32_t>(w->kills);
    h = h * 31 + static_cast<uint32_t>(w->timer);
    h = h * 31 + static_cast<uint32_t>(w->visible);
    h = h * 31 + w->controlStates.pack();

    // Weapon state
    for (int i = 0; i < NUM_WEAPONS; ++i) {
      h = h * 31 + static_cast<uint32_t>(w->weapons[i].ammo);
      h = h * 31 + static_cast<uint32_t>(w->weapons[i].delayLeft);
      h = h * 31 + static_cast<uint32_t>(w->weapons[i].loadingLeft);
      if (w->weapons[i].type) h = h * 31 + static_cast<uint32_t>(w->weapons[i].type->id);
    }

    // Ninjarope
    h = h * 31 + static_cast<uint32_t>(w->ninjarope.out);
    h = h * 31 + static_cast<uint32_t>(w->ninjarope.pos.x);
    h = h * 31 + static_cast<uint32_t>(w->ninjarope.pos.y);
  }

  // Object lists - hash count and positions
  {
    auto br = game.bobjects.begin();
    for (; br != game.bobjects.end(); ++br) {
      h = h * 31 + static_cast<uint32_t>(br->pos.x);
      h = h * 31 + static_cast<uint32_t>(br->pos.y);
    }
  }

  // Bonuses
  {
    auto r = game.bonuses.all();
    Bonus* b;
    while ((b = r.next())) {
      h = h * 31 + static_cast<uint32_t>(b->x);
      h = h * 31 + static_cast<uint32_t>(b->y);
      h = h * 31 + static_cast<uint32_t>(b->timer);
      h = h * 31 + static_cast<uint32_t>(b->weapon);
      h = h * 31 + static_cast<uint32_t>(b->frame);
    }
  }

  // SObjects
  {
    auto r = game.sobjects.all();
    SObject* s;
    while ((s = r.next())) {
      h = h * 31 + static_cast<uint32_t>(s->id);
      h = h * 31 + static_cast<uint32_t>(s->curFrame);
    }
  }

  // NObjects
  {
    auto r = game.nobjects.all();
    NObject* n;
    while ((n = r.next())) {
      h = h * 31 + static_cast<uint32_t>(n->pos.x);
      h = h * 31 + static_cast<uint32_t>(n->pos.y);
      h = h * 31 + static_cast<uint32_t>(n->vel.x);
      h = h * 31 + static_cast<uint32_t>(n->vel.y);
      h = h * 31 + static_cast<uint32_t>(n->curFrame);
      if (n->type) h = h * 31 + static_cast<uint32_t>(n->type->id);
    }
  }

  // WObjects (projectiles)
  {
    auto r = game.wobjects.all();
    WObject* wo;
    while ((wo = r.next())) {
      h = h * 31 + static_cast<uint32_t>(wo->pos.x);
      h = h * 31 + static_cast<uint32_t>(wo->pos.y);
      h = h * 31 + static_cast<uint32_t>(wo->vel.x);
      h = h * 31 + static_cast<uint32_t>(wo->vel.y);
      h = h * 31 + static_cast<uint32_t>(wo->curFrame);
      h = h * 31 + static_cast<uint32_t>(wo->timeLeft);
      if (wo->type) h = h * 31 + static_cast<uint32_t>(wo->type->id);
    }
  }

  return h;
}

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
      Worm* wA = new Worm();
      wA->settings = settings->wormSettings[idx];
      wA->health = wA->settings->health;
      wA->index = idx;
      wA->statsX = idx == 0 ? 0 : 218;

      Worm* wB = new Worm();
      wB->settings = settings->wormSettings[idx];
      wB->health = wB->settings->health;
      wB->index = idx;
      wB->statsX = idx == 0 ? 0 : 218;

      gameA->addWorm(wA);
      gameB->addWorm(wB);
    }

    // Add viewports (needed for processFrame's viewport logic)
    gameA->addViewport(new Viewport(gvl::rect(0, 0, 158, 158), 0, 504, 350));
    gameA->addViewport(new Viewport(gvl::rect(160, 0, 318, 158), 1, 504, 350));
    gameB->addViewport(new Viewport(gvl::rect(0, 0, 158, 158), 0, 504, 350));
    gameB->addViewport(new Viewport(gvl::rect(160, 0, 318, 158), 1, 504, 350));

    // Generate levels with the same RNG state
    gameA->level.generateFromSettings(*common, *settings, gameA->rand);
    gameB->level.generateFromSettings(*common, *settings, gameB->rand);

    // Initialize weapons for all worms
    for (auto* w : gameA->worms) w->initWeapons(*gameA);
    for (auto* w : gameB->worms) w->initWeapons(*gameB);

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
  void applyRandomInputs(gvl::mwc& inputRng) {
    for (int idx = 0; idx < 2; ++idx) {
      uint32_t input = inputRng() & 0x7f;  // 7 control bits
      gameA->worms[idx]->controlStates.unpack(input);
      gameB->worms[idx]->controlStates.unpack(input);
    }
  }
};

TEST_CASE("Dual simulation produces identical state", "[determinism]") {
  DualGameFixture f;

  gvl::mwc inputRng(12345);

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
    gvl::mwc inputRng(99999);

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
    Worm* w1 = new Worm();
    w1->settings = settings->wormSettings[idx];
    w1->health = w1->settings->health;
    w1->index = idx;
    game1.addWorm(w1);

    Worm* w2 = new Worm();
    w2->settings = settings->wormSettings[idx];
    w2->health = w2->settings->health;
    w2->index = idx;
    game2.addWorm(w2);
  }

  game1.addViewport(new Viewport(gvl::rect(0, 0, 158, 158), 0, 504, 350));
  game1.addViewport(new Viewport(gvl::rect(160, 0, 318, 158), 1, 504, 350));
  game2.addViewport(new Viewport(gvl::rect(0, 0, 158, 158), 0, 504, 350));
  game2.addViewport(new Viewport(gvl::rect(160, 0, 318, 158), 1, 504, 350));

  game1.level.generateFromSettings(*common, *settings, game1.rand);
  game2.level.generateFromSettings(*common, *settings, game2.rand);

  for (auto* w : game1.worms) w->initWeapons(game1);
  for (auto* w : game2.worms) w->initWeapons(game2);

  game1.paused = false;
  game2.paused = false;
  game1.startGame();
  game2.startGame();
  game1.resetWorms();
  game2.resetWorms();

  gvl::mwc inputRng1(55555);
  gvl::mwc inputRng2(55555);

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
