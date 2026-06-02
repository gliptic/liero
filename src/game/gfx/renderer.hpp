#pragma once

#include "../common.hpp"
#include "../rand.hpp"
#include "bitmap.hpp"

struct Renderer {
  Renderer() : fade_value(0) {}

  void Init(int x, int y);
  void Clear();
  void LoadPalette(Common const& common);
  void SetRenderResolution(int x, int y);

  // the bitmap that is drawn into by this renderer
  Bitmap bmp;
  Palette pal, origpal;
  int fade_value;
  // Resolution to render the game at. This should be modified via
  // setRenderResolution() to ensure that the bitmap is re-allocated
  int render_res_x = 320;
  int render_res_y = 200;
};
