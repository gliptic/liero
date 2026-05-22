#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <memory>
#include <string>

#include "common.hpp"
#include "filesystem.hpp"
#include "game.hpp"
#include "level.hpp"
#include "math.hpp"
#include "mixer/player.hpp"
#include "viewport.hpp"
#include "worm.hpp"

// TC path can be overridden via TC_PATH env var (default: data/TC/openliero)
static std::string getTcPath() {
  if (auto* p = std::getenv("TC_PATH"))
    return p;
  return "data/TC/openliero";
}

TEST_CASE("TC loads without errors", "[tc_load]") {
  auto common = std::make_shared<Common>();
  FsNode tcRoot(getTcPath());

  REQUIRE_NOTHROW(common->load(std::move(tcRoot)));

  // Basic sanity checks on loaded data
  REQUIRE(common->weapons.size() > 0);
  REQUIRE(common->nobjectTypes.size() > 0);
  REQUIRE(common->sobjectTypes.size() > 0);
  REQUIRE(common->weapOrder.size() == common->weapons.size());
}

TEST_CASE("TC supports game initialization", "[tc_load]") {
  precomputeTables();

  auto common = std::make_shared<Common>();
  FsNode tcRoot(getTcPath());
  common->load(std::move(tcRoot));

  auto settings = std::make_shared<Settings>();
  settings->lives = 5;
  settings->loadingTime = 0;
  settings->randomLevel = true;
  settings->gameMode = Settings::GMKillEmAll;

  // Clamp weapon selections to valid range for this TC
  int numWeapons = (int)common->weapons.size();
  for (int p = 0; p < 3; ++p) {
    for (int i = 0; i < Settings::selectableWeapons; ++i) {
      if ((int)settings->wormSettings[p]->weapons[i] > numWeapons)
        settings->wormSettings[p]->weapons[i] = 1;
    }
  }

  auto sp = std::make_shared<NullSoundPlayer>();
  Game game(common, settings, sp);
  game.rand.seed(42);

  for (int idx = 0; idx < 2; ++idx) {
    Worm* w = new Worm();
    w->settings = settings->wormSettings[idx];
    w->health = w->settings->health;
    w->index = idx;
    w->statsX = idx == 0 ? 0 : 218;
    game.addWorm(w);
  }

  game.addViewport(new Viewport(gvl::rect(0, 0, 158, 158), 0, 504, 350));
  game.addViewport(new Viewport(gvl::rect(160, 0, 318, 158), 1, 504, 350));

  REQUIRE_NOTHROW(game.level.generateFromSettings(*common, *settings, game.rand));

  for (auto* w : game.worms)
    REQUIRE_NOTHROW(w->initWeapons(game));

  game.paused = false;
  game.startGame();
  game.resetWorms();

  // Run a short simulation — validates no crashes during gameplay
  gvl::mwc inputRng(12345);
  constexpr int NUM_FRAMES = 200;

  for (int frame = 0; frame < NUM_FRAMES; ++frame) {
    for (int idx = 0; idx < 2; ++idx) {
      uint32_t input = inputRng() & 0x7f;
      game.worms[idx]->controlStates.unpack(input);
    }
    REQUIRE_NOTHROW(game.processFrame());
  }
}
