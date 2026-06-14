#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include "game/spectatorviewport.hpp"

namespace {

// Mirrors ComputeSpectatorZoom's whole-level fit so tests can assert against
// the same reference value without duplicating the clamp logic.
float LevelFillZoom(int render_w, int render_h, int level_w, int level_h) {
  return std::min(static_cast<float>(render_w) / static_cast<float>(level_w),
                  static_cast<float>(render_h) / static_cast<float>(level_h));
}

}  // namespace

TEST_CASE("spectator zoom stays a static fit when the level fits the window", "[spectator][zoom]") {
  // Regression: a 504x350 level inside a 1920x1080 spectator window already
  // fits whole, so the zoom must be the constant whole-level fit regardless of
  // where the worms are — driving them to opposite edges must not shrink the
  // view. Before the clamp fix, the worms-at-edges bbox dropped zoom below the
  // fill value and letterboxed the level.
  int const kRenderW = 1920;
  int const kRenderH = 1080;
  int const kLevelW = 504;
  int const kLevelH = 350;
  float const kFill = LevelFillZoom(kRenderW, kRenderH, kLevelW, kLevelH);  // ~3.0857

  // Worms close together: a tiny bounding box.
  float const kZoomClose = ComputeSpectatorZoom(kRenderW, kRenderH, 120, 120, kLevelW, kLevelH);
  // Worms at opposite corners: bbox grown past the level by the 60px margins.
  float const kZoomEdges = ComputeSpectatorZoom(kRenderW, kRenderH, 623, 469, kLevelW, kLevelH);

  CHECK(kZoomClose == Catch::Approx(kFill).epsilon(1e-4));
  CHECK(kZoomEdges == Catch::Approx(kFill).epsilon(1e-4));
  // The decisive property: zoom is independent of the bounding box once the
  // whole level already fits.
  CHECK(kZoomClose == Catch::Approx(kZoomEdges).epsilon(1e-4));
}

TEST_CASE("spectator zoom clamps to native when a large level's worms are close",
          "[spectator][zoom]") {
  // Level larger than the window; worms close → framing zoom would upscale.
  // Never upscale a level that is bigger than the window: clamp at 1.0.
  int const kRenderW = 1920;
  int const kRenderH = 1080;
  int const kLevelW = 4000;
  int const kLevelH = 3000;

  float const kZoom = ComputeSpectatorZoom(kRenderW, kRenderH, 100, 100, kLevelW, kLevelH);
  CHECK(kZoom == Catch::Approx(1.0F).epsilon(1e-4));
}

TEST_CASE("spectator zoom shows the whole level when a large level's worms are far",
          "[spectator][zoom]") {
  // Level larger than the window, bbox >= level → zoom out to the whole-level
  // fit (< 1) so both worms stay visible.
  int const kRenderW = 1920;
  int const kRenderH = 1080;
  int const kLevelW = 4000;
  int const kLevelH = 3000;
  float const kFill = LevelFillZoom(kRenderW, kRenderH, kLevelW, kLevelH);  // 0.36

  float const kZoom = ComputeSpectatorZoom(kRenderW, kRenderH, kLevelW, kLevelH, kLevelW, kLevelH);
  CHECK(kZoom == Catch::Approx(kFill).epsilon(1e-4));
  CHECK(kZoom < 1.0F);
}

TEST_CASE("spectator zoom tracks the bounding box in the mid-range", "[spectator][zoom]") {
  // Large level, bbox between "fits" and "whole level": zoom follows the
  // worm-framing value and sits strictly inside (fill, 1.0).
  int const kRenderW = 1920;
  int const kRenderH = 1080;
  int const kLevelW = 4000;
  int const kLevelH = 3000;
  float const kFill = LevelFillZoom(kRenderW, kRenderH, kLevelW, kLevelH);  // 0.36

  // bbox 3000x2160 → zoomX=0.64, zoomY=0.5 → min=0.5.
  float const kZoom = ComputeSpectatorZoom(kRenderW, kRenderH, 3000, 2160, kLevelW, kLevelH);
  CHECK(kZoom == Catch::Approx(0.5F).epsilon(1e-4));
  CHECK(kZoom > kFill);
  CHECK(kZoom < 1.0F);
}
