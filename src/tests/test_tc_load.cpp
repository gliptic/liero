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

  // Strings from tc.cfg should arrive as UTF-8. The Copyright string contains
  // 'ä' (U+00E4) which is two bytes in UTF-8 (0xC3 0xA4); historically tc.cfg
  // stored these as  (a control char), which the parser then yielded as
  // 0xC2 0x84 — neither of which decode to 'ä'. Anchor that here.
  std::string const& copy2 = common->S[SCopyright2];
  REQUIRE(copy2.find("Liero v1.33") == 0);
  REQUIRE(copy2.find("\xC3\xA4") != std::string::npos);  // 'ä' UTF-8
  REQUIRE(copy2.find("\xC2\x84") == std::string::npos);  // bogus old form
  REQUIRE(common->guessName() == "Liero v1.33");
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
    auto w = std::make_shared<Worm>();
    w->settings = settings->wormSettings[idx];
    w->health = w->settings->health;
    w->index = idx;
    w->statsX = idx == 0 ? 0 : 218;
    game.addWorm(w);
  }

  game.addViewport(new Viewport(Rect(0, 0, 158, 158), 0, 504, 350));
  game.addViewport(new Viewport(Rect(160, 0, 318, 158), 1, 504, 350));

  REQUIRE_NOTHROW(game.level.generateFromSettings(*common, *settings, game.rand));

  for (auto const& w : game.worms)
    REQUIRE_NOTHROW(w->initWeapons(game));

  game.paused = false;
  game.startGame();
  game.resetWorms();

  // Run a short simulation — validates no crashes during gameplay
  Rand inputRng(12345);
  constexpr int NUM_FRAMES = 200;

  for (int frame = 0; frame < NUM_FRAMES; ++frame) {
    for (int idx = 0; idx < 2; ++idx) {
      uint32_t input = inputRng() & 0x7f;
      game.worms[idx]->controlStates.unpack(input);
    }
    REQUIRE_NOTHROW(game.processFrame());
  }
}
