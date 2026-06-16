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

TEST_CASE("capped render resolution is a no-op when the window fits the cap",
          "[spectator][rescap]") {
  // Window shorter than the cap: render at native, byte-for-byte unchanged. This
  // is the small-window guarantee — the cap must never upscale.
  CappedRenderResolution const kR = ComputeCappedRenderResolution(1280, 800, 1080);
  CHECK(kR.w == 1280);
  CHECK(kR.h == 800);
}

TEST_CASE("capped render resolution is a no-op when the window equals the cap",
          "[spectator][rescap]") {
  // Boundary: window height exactly the cap stays untouched (no off-by-one
  // downscale of a 1080p window under a 1080 cap).
  CappedRenderResolution const kR = ComputeCappedRenderResolution(1920, 1080, 1080);
  CHECK(kR.w == 1920);
  CHECK(kR.h == 1080);
}

TEST_CASE("capped render resolution caps a 4K window preserving aspect", "[spectator][rescap]") {
  // 3840x2160 under a 1080 cap → 1920x1080: height pinned to the cap, width
  // derived from the 16:9 window aspect (3840*1080/2160 = 1920).
  CappedRenderResolution const kR = ComputeCappedRenderResolution(3840, 2160, 1080);
  CHECK(kR.h == 1080);
  CHECK(kR.w == 1920);
}

TEST_CASE("capped render resolution caps an ultrawide window preserving aspect",
          "[spectator][rescap]") {
  // 3440x1440 under a 1080 cap → 2580x1080 (3440*1080/1440 = 2580): the
  // ultrawide aspect (~2.39:1) is preserved, not squashed to 16:9.
  CappedRenderResolution const kR = ComputeCappedRenderResolution(3440, 1440, 1080);
  CHECK(kR.h == 1080);
  CHECK(kR.w == 2580);
}

TEST_CASE("capped render resolution is disabled when the cap is non-positive",
          "[spectator][rescap]") {
  // cap_h <= 0 disables the cap entirely → render at the full window.
  CappedRenderResolution const kDisabled = ComputeCappedRenderResolution(3840, 2160, 0);
  CHECK(kDisabled.w == 3840);
  CHECK(kDisabled.h == 2160);
  CappedRenderResolution const kNeg = ComputeCappedRenderResolution(3840, 2160, -1);
  CHECK(kNeg.w == 3840);
  CHECK(kNeg.h == 2160);
}

TEST_CASE("capped render resolution never collapses width below 1 pixel", "[spectator][rescap]") {
  // Degenerate guard: a sliver window under a tiny cap must still leave a >=1px
  // width the renderer can allocate.
  CappedRenderResolution const kR = ComputeCappedRenderResolution(1, 4000, 1);
  CHECK(kR.h == 1);
  CHECK(kR.w >= 1);
}

TEST_CASE("hud dirty bands always cover the bottom strip and reloading text",
          "[spectator][hudbands]") {
  // Banner hidden (-8 == hidden sentinel): only the static bands. Bottom strip
  // is the 40px stats area; reloading text sits at y=164.
  HudDirtyBands const kB = ComputeHudDirtyBands(1080, -8, -8);
  REQUIRE(kB.count == 2);
  // Bottom strip [H-40, H).
  CHECK(kB.bands[0].y == 1040);
  CHECK(kB.bands[0].h == 40);
  // Reloading text band [164, 164+8).
  CHECK(kB.bands[1].y == 164);
  CHECK(kB.bands[1].h == 8);
}

TEST_CASE("hud dirty bands add a stationary banner band when visible", "[spectator][hudbands]") {
  HudDirtyBands const kB = ComputeHudDirtyBands(1080, 100, 100);
  REQUIRE(kB.count == 3);
  // Banner glyph (8px) + its 1px shadow row → 9px tall at y=100.
  CHECK(kB.bands[2].y == 100);
  CHECK(kB.bands[2].h == 9);
}

TEST_CASE("hud dirty bands union covers a scrolling banner with no stale row",
          "[spectator][hudbands]") {
  // Banner scrolled from y=100 (prev) to y=92 (cur). The band must cover both
  // extents — [92,101) ∪ [100,109) = [92,109) — so the vacated rows are cleared.
  HudDirtyBands const kB = ComputeHudDirtyBands(1080, 92, 100);
  REQUIRE(kB.count == 3);
  HudBand const kBanner = kB.bands[2];
  CHECK(kBanner.y == 92);
  CHECK(kBanner.y + kBanner.h == 109);
  // Both the current and previous banner extents fall inside the band.
  CHECK(kBanner.y <= 92);
  CHECK(kBanner.y + kBanner.h >= 100 + 9);
}

TEST_CASE("hud dirty bands clamp to the render surface", "[spectator][hudbands]") {
  // Tiny overlay: the bottom strip clamps into [0,H) and the off-screen
  // reloading band (y=164 >= H) is dropped.
  HudDirtyBands const kB = ComputeHudDirtyBands(20, -8, -8);
  REQUIRE(kB.count == 1);
  CHECK(kB.bands[0].y == 0);
  CHECK(kB.bands[0].h == 20);
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
