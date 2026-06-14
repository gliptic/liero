#include <catch2/catch_test_macros.hpp>

#include <cstdint>

#include "game/common.hpp"
#include "game/gfx.hpp"
#include "game/gfx/blit.hpp"
#include "game/gfx/renderer.hpp"
#include "game/gfx/shadow_query.hpp"
#include "game/level.hpp"
#include "game/material.hpp"
#include "game/settings.hpp"

namespace {

// 8x8 level: every pixel is palette index 10 (a SeeShadow material) except
// column 5 which is index 20 (rock, no shadow).
struct ShadowFixture {
  Common common;
  Level level;
  Renderer renderer;

  ShadowFixture() : level(common) {
    common.materials[10].flags = Material::kSeeShadow | Material::kBackground;
    common.materials[14].flags = Material::kBackground;
    common.materials[20].flags = Material::kRock;

    level.width = 8;
    level.height = 8;
    level.material_id.assign(64, 10);
    level.materials.assign(64, common.materials[10]);
    for (int y = 0; y < 8; ++y) {
      level.material_id[5 + y * 8] = 20;
      level.materials[5 + y * 8] = common.materials[20];
    }

    renderer.Init(8, 8);
    renderer.pal.Clear();
    renderer.pal.entries[9] = {.r = 0x99, .g = 0x99, .b = 0x99, .unused = 0};
    renderer.pal.entries[10] = {.r = 0xaa, .g = 0xaa, .b = 0xaa, .unused = 0};
    renderer.pal.entries[14] = {.r = 0x11, .g = 0x22, .b = 0x33, .unused = 0};
    renderer.pal.entries[20] = {.r = 0x44, .g = 0x44, .b = 0x44, .unused = 0};
    renderer.UpdatePal32();
  }

  ShadowQuery Query(int offset_x = 0, int offset_y = 0) const {
    return ShadowQuery{.common = common,
                       .level = level,
                       .pal32 = renderer.pal32,
                       .world_offset_x = offset_x,
                       .world_offset_y = offset_y};
  }
};

}  // namespace

TEST_CASE("updatepal32 packs the working palette as ARGB8888", "[blit][pal32]") {
  Renderer renderer;
  renderer.pal.Clear();
  renderer.pal.entries[0] = {.r = 0, .g = 0, .b = 0, .unused = 0};
  renderer.pal.entries[1] = {.r = 0x12, .g = 0x34, .b = 0x56, .unused = 0};
  renderer.pal.entries[255] = {.r = 0xff, .g = 0xff, .b = 0xff, .unused = 0};

  renderer.UpdatePal32();

  // Canonical packing: alpha forced opaque, then r<<16 | g<<8 | b. This is
  // what both SDL surfaces (fixed SDL_PIXELFORMAT_ARGB8888) and the video
  // tool's BGRA byte order expect.
  REQUIRE(renderer.pal32[0] == 0xFF000000U);
  REQUIRE(renderer.pal32[1] == 0xFF123456U);
  REQUIRE(renderer.pal32[255] == 0xFFFFFFFFU);
}

TEST_CASE("updatepal32 tracks palette mutations", "[blit][pal32]") {
  Renderer renderer;
  renderer.pal.Clear();
  renderer.pal.entries[7] = {.r = 0x10, .g = 0x20, .b = 0x30, .unused = 0};
  renderer.UpdatePal32();
  REQUIRE(renderer.pal32[7] == 0xFF102030U);

  renderer.pal.entries[7] = {.r = 0x40, .g = 0x50, .b = 0x60, .unused = 0};
  renderer.UpdatePal32();
  REQUIRE(renderer.pal32[7] == 0xFF405060U);
}

TEST_CASE("shadowedargb in modern mode darkens display_data for authored seeshadow cell",
          "[blit][shadow][display-layer]") {
  ShadowFixture f;

  // Pixel (2,3): SeeShadow terrain (material 10) with an authored ARGB.
  int const kIdx = 2 + 3 * 8;
  f.level.display_data.assign(64, 0);
  f.level.display_valid.assign(64, 0);
  f.level.display_data[kIdx] = 0xFF204060U;
  f.level.display_valid[kIdx] = 1;

  ShadowQuery const kQModern{.common = f.common,
                             .level = f.level,
                             .pal32 = f.renderer.pal32,
                             .world_offset_x = 0,
                             .world_offset_y = 0,
                             .mode = ColorMode::kModern};

  // Darken: each channel >> 1; 0x20->0x10, 0x40->0x20, 0x60->0x30.
  REQUIRE(kQModern.ShadowedArgb(2, 3) == 0xFF102030U);

  // Classic mode uses pal32[material+4] = pal32[14] = 0xFF112233 from fixture.
  REQUIRE(f.Query().ShadowedArgb(2, 3) == 0xFF112233U);

  // Modern mode, pixel not authored (display_valid==0): palette fallback.
  ShadowQuery const kQModern2{.common = f.common,
                              .level = f.level,
                              .pal32 = f.renderer.pal32,
                              .world_offset_x = 0,
                              .world_offset_y = 0,
                              .mode = ColorMode::kModern};
  REQUIRE(kQModern2.ShadowedArgb(0, 3) == 0xFF112233U);  // pixel 0+3*8=24, display_valid==0
}

TEST_CASE("shadowedargb animated modern mode darkens the resolved animated color",
          "[blit][shadow][anim-layer]") {
  ShadowFixture f;

  int const kIdx = 2 + 3 * 8;
  f.level.display_data.assign(64, 0);
  f.level.display_valid.assign(64, 0);
  f.level.display_anim.assign(64, 0);
  f.level.display_valid[kIdx] = 1;
  f.level.display_anim[kIdx] = 1;
  f.level.display_data[kIdx] = 0;  // phase offset 0

  Level::ArgbRamp ramp;
  ramp.colors = {0xFF204060U, 0xFF408020U};
  ramp.shift = 0;
  f.level.argb_ramps.push_back(ramp);

  // cycles=0 → resolves to 0xFF204060 → darkened = 0xFF102030
  ShadowQuery const kQCycles0{.common = f.common,
                              .level = f.level,
                              .pal32 = f.renderer.pal32,
                              .world_offset_x = 0,
                              .world_offset_y = 0,
                              .mode = ColorMode::kModern,
                              .cycles = 0};
  REQUIRE(kQCycles0.ShadowedArgb(2, 3) == 0xFF102030U);

  // cycles=1 → resolves to 0xFF408020 → darkened = 0xFF204010
  ShadowQuery const kQCycles1{.common = f.common,
                              .level = f.level,
                              .pal32 = f.renderer.pal32,
                              .world_offset_x = 0,
                              .world_offset_y = 0,
                              .mode = ColorMode::kModern,
                              .cycles = 1};
  REQUIRE(kQCycles1.ShadowedArgb(2, 3) == 0xFF204010U);

  // Classic mode always uses the palette path, regardless of ramps.
  ShadowQuery const kQClassic{.common = f.common,
                              .level = f.level,
                              .pal32 = f.renderer.pal32,
                              .world_offset_x = 0,
                              .world_offset_y = 0,
                              .mode = ColorMode::kClassic,
                              .cycles = 1};
  REQUIRE(kQClassic.ShadowedArgb(2, 3) == 0xFF112233U);
}

TEST_CASE("shadowquery reads material from the level, not the screen", "[blit][shadow]") {
  ShadowFixture const kFixture;
  ShadowQuery const kQ = kFixture.Query();

  // SeeShadow terrain: shadowed index is the level pixel + 4.
  REQUIRE(kQ.ShadowedIndex(2, 3) == 14);
  REQUIRE(kQ.ShadowedArgb(2, 3) == 0xFF112233U);

  // Rock column: no shadow.
  REQUIRE(kQ.ShadowedIndex(5, 3) == -1);
  REQUIRE(kQ.ShadowedArgb(5, 3) == 0U);

  // Outside the level: no shadow, no crash.
  REQUIRE(kQ.ShadowedIndex(-1, 0) == -1);
  REQUIRE(kQ.ShadowedIndex(0, -1) == -1);
  REQUIRE(kQ.ShadowedIndex(8, 0) == -1);
  REQUIRE(kQ.ShadowedIndex(0, 8) == -1);
  REQUIRE(kQ.PixelAt(-1, -1) == -1);

  // The world offset maps screen to level coordinates.
  ShadowQuery const kShifted = kFixture.Query(3, 0);
  REQUIRE(kShifted.ShadowedIndex(2, 0) == -1);  // screen 2 -> level column 5
  REQUIRE(kShifted.PixelAt(2, 0) == 20);
}

TEST_CASE("blitshadowimage shadows only seeshadow terrain", "[blit][shadow]") {
  ShadowFixture f;
  ShadowQuery const kQ = f.Query();

  Bitmap& bmp = f.renderer.bmp;
  Fill(bmp, 10);
  // Old semantics keyed off the screen; paint a screen pixel with rock
  // colour over SeeShadow terrain to prove the level now decides.
  bmp.GetPixel(3, 3) = f.renderer.pal32[20];

  PalIdx const kSprite[9] = {7, 7, 7, 7, 7, 7, 7, 7, 7};
  BlitShadowImage(kQ, bmp, kSprite, 3, 2, 3, 3);

  // Level decides: (3,3) is SeeShadow terrain even though the screen
  // showed rock there.
  REQUIRE(bmp.GetPixel(3, 3) == f.renderer.pal32[14]);
  REQUIRE(bmp.GetPixel(4, 2) == f.renderer.pal32[14]);
  // Rock column untouched.
  REQUIRE(bmp.GetPixel(5, 2) == f.renderer.pal32[10]);
  REQUIRE(bmp.GetPixel(5, 3) == f.renderer.pal32[10]);
  // Outside the blit rect untouched.
  REQUIRE(bmp.GetPixel(2, 2) == f.renderer.pal32[10]);

  // Clipped blit straddling the top-left corner must not crash.
  BlitShadowImage(kQ, bmp, kSprite, -1, -1, 3, 3);
  REQUIRE(bmp.GetPixel(0, 0) == f.renderer.pal32[14]);
}

TEST_CASE("drawshadowline shadows along the line from level material", "[blit][shadow]") {
  ShadowFixture f;
  ShadowQuery const kQ = f.Query();

  Bitmap& bmp = f.renderer.bmp;
  Fill(bmp, 10);

  DrawShadowLine(kQ, bmp, 0, 1, 7, 1);

  // DO_LINE never paints the line's start point.
  REQUIRE(bmp.GetPixel(0, 1) == f.renderer.pal32[10]);
  for (int x = 1; x < 8; ++x) {
    if (x == 5) {
      REQUIRE(bmp.GetPixel(x, 1) == f.renderer.pal32[10]);  // rock column skipped
    } else {
      REQUIRE(bmp.GetPixel(x, 1) == f.renderer.pal32[14]);
    }
  }
  REQUIRE(bmp.GetPixel(3, 0) == f.renderer.pal32[10]);  // other rows untouched
}

TEST_CASE("blitimager draws only where the level pixel is in range", "[blit][shadow]") {
  ShadowFixture f;

  // Range check is [160, 168) against the level pixel.
  f.level.material_id[3 + 3 * 8] = 160;
  f.level.material_id[4 + 3 * 8] = 167;
  f.level.material_id[2 + 3 * 8] = 168;  // out of range

  ShadowQuery const kQ = f.Query();
  Bitmap& bmp = f.renderer.bmp;
  Fill(bmp, 0);

  PalIdx const kSprite[9] = {9, 9, 9, 9, 9, 9, 9, 9, 9};
  BlitImageR(kQ, bmp, kSprite, 2, 2, 3, 3);

  REQUIRE(bmp.GetPixel(3, 3) == f.renderer.pal32[9]);
  REQUIRE(bmp.GetPixel(4, 3) == f.renderer.pal32[9]);
  REQUIRE(bmp.GetPixel(2, 3) == f.renderer.pal32[0]);  // 168: out of range
  REQUIRE(bmp.GetPixel(3, 2) == f.renderer.pal32[0]);  // level pixel 10: out of range
}

TEST_CASE("widened bitmap stores pal32-resolved argb", "[blit][argb]") {
  Renderer renderer;
  renderer.Init(8, 8);
  renderer.pal.Clear();
  renderer.pal.entries[5] = {.r = 0x10, .g = 0x20, .b = 0x30, .unused = 0};
  renderer.UpdatePal32();

  // The bitmap resolves indices through its renderer's frame-scope LUT.
  REQUIRE(renderer.bmp.pal32 == renderer.pal32);

  Fill(renderer.bmp, 5);
  REQUIRE(renderer.bmp.GetPixel(0, 0) == 0xFF102030U);
  REQUIRE(renderer.bmp.GetPixel(7, 7) == 0xFF102030U);

  renderer.bmp.SetPixel(2, 2, 0);
  REQUIRE(renderer.bmp.GetPixel(2, 2) == 0xFF000000U);

  DrawLine(renderer.bmp, 0, 4, 4, 4, 5);
  REQUIRE(renderer.bmp.GetPixel(2, 4) == 0xFF102030U);
}

TEST_CASE("appearanceat resolves level pixels through pal32", "[blit][argb]") {
  ShadowFixture f;
  f.renderer.pal.entries[10] = {.r = 4, .g = 5, .b = 6, .unused = 0};
  f.renderer.UpdatePal32();

  REQUIRE(f.level.AppearanceAt(0, f.renderer.mode, f.renderer.pal32, 0) == 0xFF040506U);
}

TEST_CASE("drawlevel paints terrain and blitbitmap restores argb", "[blit][argb]") {
  ShadowFixture f;
  f.renderer.pal.entries[10] = {.r = 1, .g = 2, .b = 3, .unused = 0};
  f.renderer.pal.entries[20] = {.r = 7, .g = 8, .b = 9, .unused = 0};
  f.renderer.UpdatePal32();

  DrawLevel(f.renderer.bmp, f.level, 0, 0);
  REQUIRE(f.renderer.bmp.GetPixel(0, 0) == f.renderer.pal32[10]);
  REQUIRE(f.renderer.bmp.GetPixel(5, 2) == f.renderer.pal32[20]);

  // frozen-screen restore path: ARGB -> ARGB, no palette involved.
  Bitmap frozen;
  frozen.Copy(f.renderer.bmp);
  Fill(f.renderer.bmp, 0);
  BlitBitmap(f.renderer.bmp, frozen, 4, 0, 4, 8);
  REQUIRE(f.renderer.bmp.GetPixel(5, 2) == f.renderer.pal32[20]);
  REQUIRE(f.renderer.bmp.GetPixel(0, 0) == f.renderer.pal32[0]);
}

TEST_CASE("scaledraw magnifies argb and applies the composition fade", "[blit][argb]") {
  uint32_t const kSrc[4] = {0xFF204060U, 0xFF000000U, 0xFFFFFFFFU, 0xFF102030U};

  uint32_t dest[16] = {};
  ScaleDraw(kSrc, 2, 2, 2, reinterpret_cast<uint8_t*>(dest), 4 * sizeof(uint32_t), 2,
            /*fade=*/33);
  REQUIRE(dest[0] == 0xFF204060U);
  REQUIRE(dest[1] == 0xFF204060U);
  REQUIRE(dest[2] == 0xFF000000U);
  REQUIRE(dest[4] == 0xFF204060U);
  REQUIRE(dest[8] == 0xFFFFFFFFU);
  REQUIRE(dest[9] == 0xFFFFFFFFU);
  REQUIRE(dest[10] == 0xFF102030U);
  REQUIRE(dest[15] == 0xFF102030U);

  // Half fade: each channel becomes (v * 16) >> 5, the Palette::Fade math.
  uint32_t dest1[4] = {};
  ScaleDraw(kSrc, 2, 2, 2, reinterpret_cast<uint8_t*>(dest1), 2 * sizeof(uint32_t), 1,
            /*fade=*/16);
  REQUIRE(dest1[0] == 0xFF102030U);
  REQUIRE(dest1[3] == 0xFF081018U);

  // Fade 0 is black, alpha stays opaque.
  ScaleDraw(kSrc, 2, 2, 2, reinterpret_cast<uint8_t*>(dest1), 2 * sizeof(uint32_t), 1,
            /*fade=*/0);
  REQUIRE(dest1[0] == 0xFF000000U);
}

TEST_CASE("scaledrawarea 1x1 src to 1x1 dest copies the pixel", "[blit][scaledrawarea]") {
  uint32_t const kSrc = 0xFF102030U;
  uint32_t dest = 0;
  ScaleDrawArea(&kSrc, 1, 1, 1, &dest, 1, 1, 1);
  REQUIRE(dest == 0xFF102030U);
}

TEST_CASE("scaledrawarea 2x2 src to 1x1 dest averages all four pixels", "[blit][scaledrawarea]") {
  // Four pixels: R channels 0x10, 0x30, 0x50, 0x70 — avg = 0x40.
  uint32_t const kSrc[4] = {0xFF100000U, 0xFF300000U, 0xFF500000U, 0xFF700000U};
  uint32_t dest = 0;
  ScaleDrawArea(kSrc, 2, 2, 2, &dest, 1, 1, 1);
  REQUIRE(dest == 0xFF400000U);
}

TEST_CASE("scaledrawarea 4x1 src to 2x1 dest averages pairs", "[blit][scaledrawarea]") {
  // Two pairs: (0x04, 0x08) avg 0x06; (0x10, 0x20) avg 0x18.
  uint32_t const kSrc[4] = {0xFF040000U, 0xFF080000U, 0xFF100000U, 0xFF200000U};
  uint32_t dest[2] = {};
  ScaleDrawArea(kSrc, 4, 1, 4, dest, 2, 1, 2);
  REQUIRE(dest[0] == 0xFF060000U);
  REQUIRE(dest[1] == 0xFF180000U);
}

TEST_CASE("scaledrawarea 1x4 src to 1x2 dest averages column pairs", "[blit][scaledrawarea]") {
  // Vertical: (0x04, 0x08) avg 0x06; (0x20, 0x40) avg 0x30.
  uint32_t const kSrc[4] = {0xFF000004U, 0xFF000008U, 0xFF000020U, 0xFF000040U};
  uint32_t dest[2] = {};
  ScaleDrawArea(kSrc, 1, 4, 1, dest, 1, 2, 1);
  REQUIRE(dest[0] == 0xFF000006U);
  REQUIRE(dest[1] == 0xFF000030U);
}

TEST_CASE("updatemenupalettes repacks pal32 with the menu animation", "[blit][pal32]") {
  // Regression: the menu palette rebuild must end with an UpdatePal32 (and
  // must not compose) — menus draw through pal32, so a stale LUT freezes
  // the selected-item blink (rotated entry 168) and worm colour previews.
  gfx.settings = std::make_shared<Settings>();
  gfx.play_renderer.Init(8, 8);
  gfx.single_screen_renderer.Init(8, 8);

  for (int i = 0; i < 256; ++i) {
    auto const kV = static_cast<uint8_t>(i);
    gfx.play_renderer.origpal.entries[i] = {.r = kV, .g = kV, .b = kV, .unused = 0};
  }
  gfx.single_screen_renderer.origpal = gfx.play_renderer.origpal;

  auto pack = [](Color const& c) {
    return 0xFF000000U | (static_cast<uint32_t>(c.r) << 16) | (static_cast<uint32_t>(c.g) << 8) |
           c.b;
  };

  gfx.UpdateMenuPalettes();
  uint32_t const kFirst = gfx.play_renderer.pal32[168];
  REQUIRE(kFirst == pack(gfx.play_renderer.pal.entries[168]));
  REQUIRE(gfx.single_screen_renderer.pal32[168] ==
          pack(gfx.single_screen_renderer.pal.entries[168]));

  // The next frame's rebuild advances the water rotation under the
  // selected-item colour.
  gfx.UpdateMenuPalettes();
  REQUIRE(gfx.play_renderer.pal32[168] != kFirst);
  REQUIRE(gfx.play_renderer.pal32[168] == pack(gfx.play_renderer.pal.entries[168]));

  // Live worm-colour edits must reach pal32 on the same frame.
  gfx.settings->worm_settings[0]->rgb[0] = 252;
  gfx.settings->worm_settings[0]->rgb[1] = 0;
  gfx.settings->worm_settings[0]->rgb[2] = 0;
  gfx.UpdateMenuPalettes();
  int const kBase = Palette::kWormColorBlocks[0].base;
  REQUIRE(gfx.play_renderer.pal32[kBase] == pack(gfx.play_renderer.pal.entries[kBase]));
}

TEST_CASE("scaledrawarea upscales small src to fill larger dest", "[blit][scaledrawarea]") {
  // Verifies the kCount==0 nearest-neighbour path used when dest > src.
  // SpectatorViewport::Draw relies on this to upscale a small level to fill
  // a large spectator window.
  uint32_t const kSrc[2] = {0xFF112233U, 0xFF445566U};
  uint32_t dest[8] = {};
  ScaleDrawArea(kSrc, 2, 1, 2, dest, 4, 2, 4);
  // Left two columns map to src[0], right two to src[1], both rows.
  REQUIRE(dest[0] == 0xFF112233U);
  REQUIRE(dest[1] == 0xFF112233U);
  REQUIRE(dest[2] == 0xFF445566U);
  REQUIRE(dest[3] == 0xFF445566U);
  REQUIRE(dest[4] == 0xFF112233U);
  REQUIRE(dest[5] == 0xFF112233U);
  REQUIRE(dest[6] == 0xFF445566U);
  REQUIRE(dest[7] == 0xFF445566U);
}

TEST_CASE("spectator-resize: freeze-restore must not shrink renderer bmp",
          "[blit][spectator-resize]") {
  // Regression for resize-while-paused segfault (introduced a634c3b).
  // After OnWindowResize, SetRenderResolution sets render_res=1920x1080 and
  // bmp.pitch=1920.  DrawSpectatorInfo (and WeaponSelection::DrawSpectatorViewports)
  // called bmp.Copy(frozen_spectator_screen), which shrank bmp.pitch back to
  // the frozen screen's pre-resize pitch (640).  Flip() then called
  // ScaleDraw(bmp.pixels, render_res_x=1920, render_res_y=1080, pitch=640,...),
  // which reads at row y*640 for y up to 1079, accessing offset 690560 in a
  // 256000-element array — out-of-bounds read — segfault.
  //
  // The fix: replace bmp.Copy(frozen) with Fill(bmp,0)+BlitBitmap so the bmp
  // pitch/dimensions are preserved at the render resolution.
  Renderer renderer;
  renderer.Init(1920, 1080);

  Bitmap frozen;
  frozen.Alloc(640, 400);

  // Correct restore path (the fix): Fill keeps bmp at render resolution;
  // BlitBitmap copies the frozen content without resizing.
  Fill(renderer.bmp, 0);
  if (frozen.pixels != nullptr) {
    BlitBitmap(renderer.bmp, frozen, 0, 0, frozen.w, frozen.h);
  }

  // ScaleDraw reads at y*pitch for y in [0, render_res_y), so pitch must
  // equal render_res_x to stay in bounds.
  REQUIRE(renderer.bmp.pitch == static_cast<unsigned int>(renderer.render_res_x));
  REQUIRE(renderer.bmp.h == renderer.render_res_y);
}
