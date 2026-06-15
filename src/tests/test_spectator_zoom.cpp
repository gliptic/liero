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

TEST_CASE("spectator composite dest rect fills the window at zoom 1", "[spectator][composite]") {
  // Scratch exactly the render size, zoom 1 → no scaling, no letterbox bars.
  SpectatorDstRect const kDst = ComputeSpectatorDstRect(1280, 800, 1280, 800, 1.0F);
  CHECK(kDst.x == 0);
  CHECK(kDst.y == 0);
  CHECK(kDst.w == 1280);
  CHECK(kDst.h == 800);
}

TEST_CASE("spectator composite dest rect centres a zoomed-out world", "[spectator][composite]") {
  // 1000x1000 scratch at zoom 0.5 → 500x500 output centred in a 1280x800
  // window: x=(1280-500)/2, y=(800-500)/2.
  SpectatorDstRect const kDst = ComputeSpectatorDstRect(1280, 800, 1000, 1000, 0.5F);
  CHECK(kDst.w == 500);
  CHECK(kDst.h == 500);
  CHECK(kDst.x == 390);
  CHECK(kDst.y == 150);
}

TEST_CASE("spectator composite dest rect clamps to the render surface", "[spectator][composite]") {
  // A computed output larger than the render surface is clamped to it (it must
  // never overhang), pinning the corresponding offset to 0.
  SpectatorDstRect const kDst = ComputeSpectatorDstRect(1280, 800, 4096, 1000, 0.5F);
  CHECK(kDst.w == 1280);  // 4096*0.5 = 2048, clamped to 1280
  CHECK(kDst.x == 0);
  CHECK(kDst.h == 500);
  CHECK(kDst.y == 150);
}

TEST_CASE("world pass renders 1:1 when not zoomed out", "[spectator][worldpass]") {
  // zoom >= 1 (small map upscaled, or native): scale is 1.0 and the scratch is
  // the visible world region at native pixels — the existing 1:1 path, so no
  // behaviour change for small maps.
  WorldPassScratch const kP = ComputeWorldPassScratch(1280, 800, 2.0F, 504, 350);
  CHECK(kP.scale == Catch::Approx(1.0F));
  // Visible region = min(render/zoom, level) = min(640, 504) x min(400, 350).
  CHECK(kP.w == 504);
  CHECK(kP.h == 350);
}

TEST_CASE("world pass downscales to output resolution when zoomed out", "[spectator][worldpass]") {
  // 4096^2 level, 1280x800 window, worms apart -> zoom ~0.195. The 1:1 scratch
  // would be the whole 4096^2 (~16.7 Mpx); the downscaled pass renders at
  // ~output resolution instead, bounding cost by the window.
  float const kZoom = std::min(1280.0F / 4096.0F, 800.0F / 4096.0F);  // 0.195
  WorldPassScratch const kP = ComputeWorldPassScratch(1280, 800, kZoom, 4096, 4096);
  CHECK(kP.scale == Catch::Approx(kZoom));
  // Scratch is the visible region (= whole level here) scaled by `scale`, so it
  // is bounded by the render surface, not the level.
  CHECK(kP.w <= 1280);
  CHECK(kP.h <= 800);
  CHECK(kP.w == static_cast<int>(std::lroundf(4096.0F * kZoom)));
  // Far smaller than the 1:1 scratch it replaces.
  CHECK(kP.w < 4096);
  CHECK(kP.h < 4096);
}

TEST_CASE("world pass scratch never collapses below 1 pixel", "[spectator][worldpass]") {
  // Degenerate guard: extreme zoom-out on a tiny region must still leave a
  // 1x1 scratch the GPU can sample.
  WorldPassScratch const kP = ComputeWorldPassScratch(1280, 800, 0.001F, 10, 10);
  CHECK(kP.w >= 1);
  CHECK(kP.h >= 1);
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
