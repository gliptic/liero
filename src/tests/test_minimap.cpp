#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>

#include "game/common.hpp"
#include "game/gfx/blit.hpp"
#include "game/gfx/renderer.hpp"
#include "game/level.hpp"

namespace {

// Minimal fixture: a level of given dimensions with uniform material 1, rendered
// into a bitmap sized to bmp_w × bmp_h pre-filled with a background colour so
// drawn pixels are distinguishable from undrawn ones.
struct MinimapFixture {
  Common common;
  Level level;
  Renderer renderer;
  int bmp_w;
  int bmp_h;

  MinimapFixture(int level_w, int level_h, int bitmap_w, int bitmap_h)
      : level(common), bmp_w(bitmap_w), bmp_h(bitmap_h) {
    level.width = level_w;
    level.height = level_h;
    // All level pixels use palette index 1 (foreground).
    level.material_id.assign(static_cast<std::size_t>(level_w) * level_h, 1);

    renderer.Init(bitmap_w, bitmap_h);
    renderer.pal.Clear();
    // index 1 → red (foreground — written by DrawMiniature)
    // index 2 → blue (background — pre-fill sentinel)
    renderer.pal.entries[1] = {.r = 0xff, .g = 0x00, .b = 0x00, .unused = 0};
    renderer.pal.entries[2] = {.r = 0x00, .g = 0x00, .b = 0xff, .unused = 0};
    renderer.UpdatePal32();
    Fill(renderer.bmp, 2);  // pre-fill with background sentinel
  }

  // Count columns in row y that received a foreground pixel.
  int ColsInRow(int y) const {
    int n = 0;
    for (int x = 0; x < bmp_w; ++x) {
      if (renderer.bmp.GetPixel(x, y) == renderer.pal32[1]) {
        ++n;
      }
    }
    return n;
  }

  // Count rows in column x that received a foreground pixel.
  int RowsInCol(int x) const {
    int n = 0;
    for (int y = 0; y < bmp_h; ++y) {
      if (renderer.bmp.GetPixel(x, y) == renderer.pal32[1]) {
        ++n;
      }
    }
    return n;
  }

  // Count all foreground pixels in the bitmap.
  int TotalDrawn() const {
    int n = 0;
    for (int y = 0; y < bmp_h; ++y) {
      for (int x = 0; x < bmp_w; ++x) {
        if (renderer.bmp.GetPixel(x, y) == renderer.pal32[1]) {
          ++n;
        }
      }
    }
    return n;
  }
};

}  // namespace

// ── Step-formula tests (pure arithmetic) ──────────────────────────────────────
// The call sites compute:  step = max((dim + target - 1) / target, 1)
// These cases lock in that the constants produce the original hardcoded values
// for a standard 504×350 level.

TEST_CASE("hud minimap step formula yields 10 for a 504x350 level", "[minimap][step]") {
  REQUIRE(std::max((504 + Level::kHudMinimapW - 1) / Level::kHudMinimapW, 1) == 10);
  REQUIRE(std::max((350 + Level::kHudMinimapH - 1) / Level::kHudMinimapH, 1) == 10);
}

TEST_CASE("spectator minimap step formula yields 2 for a 504x350 level", "[minimap][step]") {
  REQUIRE(std::max((504 + Level::kSpecMinimapW - 1) / Level::kSpecMinimapW, 1) == 2);
  REQUIRE(std::max((350 + Level::kSpecMinimapH - 1) / Level::kSpecMinimapH, 1) == 2);
}

TEST_CASE("hud minimap step doubles for a 1008x700 level", "[minimap][step]") {
  REQUIRE(std::max((1008 + Level::kHudMinimapW - 1) / Level::kHudMinimapW, 1) == 20);
  REQUIRE(std::max((700 + Level::kHudMinimapH - 1) / Level::kHudMinimapH, 1) == 20);
}

TEST_CASE("spectator minimap step doubles for a 1008x700 level", "[minimap][step]") {
  REQUIRE(std::max((1008 + Level::kSpecMinimapW - 1) / Level::kSpecMinimapW, 1) == 4);
  REQUIRE(std::max((700 + Level::kSpecMinimapH - 1) / Level::kSpecMinimapH, 1) == 4);
}

// ── DrawMiniature dimension tests ─────────────────────────────────────────────
// Verify that DrawMiniature draws the expected column and row counts for the
// canonical 504×350 level — the numbers that match original Liero 1.36.

TEST_CASE("drawminiature step=10 draws 50 cols and 35 rows for a 504x350 level",
          "[minimap][drawminiature]") {
  MinimapFixture f(504, 350, 52, 36);
  f.level.DrawMiniature(f.renderer.bmp, 0, 0, 10, 10);
  REQUIRE(f.ColsInRow(0) == 50);
  REQUIRE(f.RowsInCol(0) == 35);
}

TEST_CASE("drawminiature step=2 draws 252 cols and 175 rows for a 504x350 level",
          "[minimap][drawminiature]") {
  MinimapFixture f(504, 350, 252, 175);
  f.level.DrawMiniature(f.renderer.bmp, 0, 0, 2, 2);
  REQUIRE(f.ColsInRow(0) == 252);
  REQUIRE(f.RowsInCol(0) == 175);
}

TEST_CASE("drawminiature step=2 fills the spectator target exactly for a 504x350 level",
          "[minimap][drawminiature]") {
  // 504/2 = 252 and 350/2 = 175 — the spectator minimap has zero wasted pixels.
  MinimapFixture f(504, 350, 252, 175);
  f.level.DrawMiniature(f.renderer.bmp, 0, 0, 2, 2);
  REQUIRE(f.TotalDrawn() == 252 * 175);
}

// ── Fixed spectator layout regression ─────────────────────────────────────────
// Static spectator screens (pause/SETUP/waiting/weapon-select) use a fixed
// 640x400 logical buffer that SDL letterbox-scales to the window, matching
// the original Liero behaviour.  These constants lock in the correct position
// so any accidental regression (e.g. rendering into native-res instead) is
// caught by the pixel-placement check below.

TEST_CASE("spectator fixed layout is 640x400 and minimap fits within it for a 504x350 level",
          "[minimap][layout]") {
  constexpr int kFixedW = 640;
  constexpr int kFixedH = 400;
  constexpr int kCenterX = kFixedW / 2;
  constexpr int kMinimapX = kCenterX - Level::kSpecMinimapW / 2;  // 320 - 126 = 194
  constexpr int kMinimapY = kFixedH - 208;                        // 192

  // Minimap must fit entirely within the fixed buffer.
  STATIC_REQUIRE(kMinimapX >= 0);
  STATIC_REQUIRE(kMinimapY >= 0);
  STATIC_REQUIRE(kMinimapX + Level::kSpecMinimapW <= kFixedW);
  STATIC_REQUIRE(kMinimapY + Level::kSpecMinimapH <= kFixedH);

  // DrawMiniature must place pixels at the computed position.
  MinimapFixture f(504, 350, kFixedW, kFixedH);
  int const kStepX = std::max((504 + Level::kSpecMinimapW - 1) / Level::kSpecMinimapW, 1);
  int const kStepY = std::max((350 + Level::kSpecMinimapH - 1) / Level::kSpecMinimapH, 1);
  f.level.DrawMiniature(f.renderer.bmp, kMinimapX, kMinimapY, kStepX, kStepY);
  REQUIRE(f.renderer.bmp.GetPixel(kMinimapX, kMinimapY) == f.renderer.pal32[1]);
  REQUIRE(f.renderer.bmp.GetPixel(0, 0) == f.renderer.pal32[2]);  // top-left: no minimap pixel
}
