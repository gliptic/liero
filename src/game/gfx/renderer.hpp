#pragma once

#include "../common.hpp"
#include "../rand.hpp"
#include "bitmap.hpp"

struct Renderer {
  Renderer() = default;

  void Init(int x, int y);
  void Clear();
  void LoadPalette(Common const& common);
  void SetRenderResolution(int x, int y);
  // Repacks `pal` into `pal32`. Every palette-rebuild block must end with
  // this, and must run before anything draws into `bmp` for the frame —
  // blits resolve palette indices through `pal32` at draw time.
  void UpdatePal32();

  // The palette `pal` is rebuilt from every frame, picked by `mode`.
  Palette const& Origpal() const { return mode == ColorMode::kModern ? origpal_modern : origpal; }

  // the bitmap that is drawn into by this renderer
  Bitmap bmp;
  Palette pal;
  // ARGB8888 (0xFF000000 | r<<16 | g<<8 | b) image of `pal`, frame scope.
  uint32_t pal32[256] = {};
  // Classic palette origin: the EXE/TC palette, or a level's custom palette.
  Palette origpal;
  // Modern palette origin: the TC's modern.pal (or a full-range expansion of
  // the classic palette when no modern.pal ships).
  Palette origpal_modern;
  // Live colour mode of this renderer; Settings hold the default, each
  // renderer can be switched independently (e.g. spectator vs play screen).
  ColorMode mode{ColorMode::kClassic};
  int fade_value{0};
  // Resolution to render the game at. This should be modified via
  // setRenderResolution() to ensure that the bitmap is re-allocated
  int render_res_x = 320;
  int render_res_y = 200;
};
