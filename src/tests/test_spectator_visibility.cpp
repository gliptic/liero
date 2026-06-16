#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <memory>

#include "common.hpp"
#include "filesystem.hpp"
#include "game.hpp"
#include "level.hpp"
#include "math.hpp"
#include "mixer/player.hpp"
#include "settings.hpp"
#include "spectatorviewport.hpp"
#include "worm.hpp"

// Drives a real Game + SpectatorViewport::Process at a capped spectator render
// resolution and asserts that, wherever the two worms are, they both land inside
// the world region the spectator actually draws. Reproduces the report that with
// PR8's render-resolution cap a worm can be moved off the spectator screen.
namespace {

struct SpectatorFixture {
  std::shared_ptr<Common> common;
  std::shared_ptr<Settings> settings;
  std::shared_ptr<SoundPlayer> sound;
  std::unique_ptr<Game> game;
  SpectatorViewport vp{Rect(0, 0, 640, 400)};

  SpectatorFixture(int level_w, int level_h) {
    PrecomputeTables();
    common = std::make_shared<Common>();
    common->load(FsNode("data") / "TC" / "openliero");

    settings = std::make_shared<Settings>();
    settings->random_level = true;
    settings->random_map_width = level_w;
    settings->random_map_height = level_h;
    settings->game_mode = Settings::kGmKillEmAll;

    sound = std::make_shared<NullSoundPlayer>();
    game = std::make_unique<Game>(common, settings, sound);
    game->rand.Seed(42);

    for (int idx = 0; idx < 2; ++idx) {
      auto w = std::make_shared<Worm>();
      w->settings = settings->worm_settings[idx];
      w->health = w->settings->health;
      w->index = idx;
      w->visible = true;
      game->AddWorm(w);
    }

    game->level.GenerateFromSettings(*common, *settings, game->rand);
    for (auto const& w : game->worms) {
      w->InitWeapons(*game);
    }

    game->AddSpectatorViewport(&vp);
  }

  void SetRenderResolution(int w, int h) {
    vp.render_w = w;
    vp.render_h = h;
  }

  void PlaceWorm(int idx, int x, int y) const { game->worms[idx]->pos = Itof(IVec2(x, y)); }

  // Reproduces the visible world region the Draw path actually samples and
  // composites (see SpectatorViewport::Draw): the [x, x+kViewW) x [y, y+kViewH)
  // rectangle, where kViewW/kViewH are clamped to the level like Draw does.
  void CheckBothWormsVisible(const char* label) {
    vp.Process(*game);
    int const kViewW =
        std::min(static_cast<int>(static_cast<float>(vp.render_w) / vp.zoom), game->level.width);
    int const kViewH =
        std::min(static_cast<int>(static_cast<float>(vp.render_h) / vp.zoom), game->level.height);
    for (int idx = 0; idx < 2; ++idx) {
      int const kWx = Ftoi(game->worms[idx]->pos.x);
      int const kWy = Ftoi(game->worms[idx]->pos.y);
      INFO(label << " worm " << idx << " at (" << kWx << "," << kWy << ") view=[" << vp.x << ","
                 << (vp.x + kViewW) << ")x[" << vp.y << "," << (vp.y + kViewH)
                 << ") zoom=" << vp.zoom << " render=" << vp.render_w << "x" << vp.render_h
                 << " level=" << game->level.width << "x" << game->level.height);
      CHECK(kWx >= vp.x);
      CHECK(kWx < vp.x + kViewW);
      CHECK(kWy >= vp.y);
      CHECK(kWy < vp.y + kViewH);
    }
  }
};

}  // namespace

TEST_CASE("spectator keeps both worms visible on a large map (capped res)",
          "[spectator][visibility]") {
  // Large map, capped 1920x1080 spectator render surface (e.g. a 4K window).
  SpectatorFixture f(2000, 2000);
  f.SetRenderResolution(1920, 1080);

  // Worms close together near the centre.
  f.PlaceWorm(0, 980, 980);
  f.PlaceWorm(1, 1020, 1020);
  f.CheckBothWormsVisible("close");

  // Worms moderately apart.
  f.PlaceWorm(0, 400, 400);
  f.PlaceWorm(1, 1600, 1600);
  f.CheckBothWormsVisible("mid");

  // Worms at opposite corners — the worst case for zoom-out.
  f.PlaceWorm(0, 30, 30);
  f.PlaceWorm(1, 1970, 1970);
  f.CheckBothWormsVisible("corners");

  // One worm hugging an edge while the other is far away.
  f.PlaceWorm(0, 1990, 1000);
  f.PlaceWorm(1, 50, 1000);
  f.CheckBothWormsVisible("horizontal-spread");
}

TEST_CASE("spectator keeps both worms visible on a wide map (capped res)",
          "[spectator][visibility]") {
  SpectatorFixture f(4000, 700);
  f.SetRenderResolution(1920, 1080);

  f.PlaceWorm(0, 50, 350);
  f.PlaceWorm(1, 3950, 350);
  f.CheckBothWormsVisible("wide-extremes");
}
