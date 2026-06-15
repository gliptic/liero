#pragma once

#include <algorithm>
#include <vector>
#include "color.hpp"
#include "math/rect.hpp"
#include "sprite.hpp"

struct Level;
struct Common;
struct Rand;
struct Bitmap;
struct ShadowQuery;

void Vline(Bitmap& scr, int x, int y1, int y2, int color);

void FillRect(Bitmap& scr, int x, int y, int w, int h, int color);
void Fill(Bitmap& scr, int color);
// Clears `scr` to fully transparent (ARGB 0x00000000) rather than to a palette
// colour. Used for the spectator HUD overlay layer (PR7 Task 1c), which must
// blend over the GPU-scaled world: only the drawn HUD pixels (opaque pal32)
// show, everything else is see-through.
void FillTransparent(Bitmap& scr);
void DrawBar(Bitmap& scr, int x, int y, int width, int color);
void DrawBar(Bitmap& scr, int x, int y, int width, int height, int color);
void DrawRoundedBox(Bitmap& scr, int x, int y, int color, int height, int width);
void DrawRoundedLineBox(Bitmap& scr, int x, int y, int color, int width, int height);
// Paints the level's appearance into the screen (the terrain draw).
void DrawLevel(Bitmap& scr, Level const& level, int x, int y);

// Downscaled terrain render for the zoomed-out spectator world pass (PR7
// Task 1): fills `scr` (sized to ~output resolution) by nearest-sampling the
// level, so terrain cost is bounded by the window, not the level area. Scratch
// pixel (px,py) samples world ((view_x,view_y) + (px,py)/scale). `scale` < 1.
void DrawLevelScaled(Bitmap& scr, Level const& level, int view_x, int view_y, float scale);

// Nearest-neighbour scaled sprite blit (transparent: palette index 0 skipped)
// for the downscaled spectator world pass. (x,y) is the already-scaled
// top-left in `scr`; the sprite is drawn at `scale` of its native size.
void BlitImageScaled(Bitmap& scr, Sprite spr, int x, int y, float scale);
// ARGB rectangle copy at identical coordinates (frozen_screen restores).
void BlitBitmap(Bitmap& scr, Bitmap const& src, int x, int y, int width, int height);
void BlitImage(Bitmap& scr, Sprite spr, int x, int y);
void BlitImageR(ShadowQuery const& shadow, Bitmap& scr, const PalIdx* mem, int x, int y, int width,
                int height);
void BlitImageTrans(Bitmap& scr, Sprite spr, int x, int y, int phase);
void BlitShadowImage(ShadowQuery const& shadow, Bitmap& scr, const PalIdx* mem, int x, int y,
                     int width, int height);
void BlitStone(Common& common, Level& level, bool p1, const PalIdx* mem, int x, int y);
void BlitFireCone(Bitmap& scr, int fc, PalIdx* mem, int x, int y);
void DrawDirtEffect(Common& common, Rand& rand, Level& level, int dirt_effect, int x, int y);
void BlitImageOnMap(Common& common, Level& level, PalIdx* mem, int x, int y, int width, int height);
void CorrectShadow(Common& common, Level& level, Rect rect);
void DrawDashedLineBox(Bitmap& scr, int x, int y, int color, int color2, int num, int den,
                       int width, int height, int phase);

void DrawNinjarope(Common& common, Bitmap& scr, int from_x, int from_y, int to_x, int to_y);
void DrawLaserSight(Bitmap& scr, Rand& rand, int from_x, int from_y, int to_x, int to_y);
void DrawShadowLine(ShadowQuery const& shadow, Bitmap& scr, int from_x, int from_y, int to_x,
                    int to_y);
void DrawLine(Bitmap& scr, int from_x, int from_y, int to_x, int to_y, int color);

void DrawGraph(Bitmap& scr, std::vector<double> const& data, int height, int start_x, int start_y,
               int color, int neg_color, bool balanced);

// Nearest-neighbor magnification to an ARGB destination, applying the
// renderer's fade at composition time (fade >= 32 is identity).
void ScaleDraw(uint32_t const* src, int w, int h, std::size_t src_pitch, uint8_t* dest,
               std::size_t dest_pitch, int mag, int fade);

// Box-filter (area-averaging) downscale from ARGB src to ARGB dest.
// Pitches are in uint32_t units (pixels, not bytes).
void ScaleDrawArea(uint32_t const* src, int src_w, int src_h, std::size_t src_pitch, uint32_t* dest,
                   int dest_w, int dest_h, std::size_t dest_pitch);

int FitScreen(int back_w, int back_h, int scr_w, int scr_h, int& offset_x, int& offset_y);

struct Heatmap {
  Heatmap(int width, int height, int org_width, int org_height)
      : width(width),
        height(height),
        org_width(org_width),
        org_height(org_height),
        map(width * height) {}

  void Inc(int x, int y, int v = 1) {
    x = (x * width) / org_width;
    y = (y * height) / org_height;
    x = std::min(std::max(x, 0), width - 1);
    y = std::min(std::max(y, 0), height - 1);

    map[y * width + x] += v;
  }

  void IncArea(int x, int y, int v = 1) {
    x = (x * width) / org_width;
    y = (y * height) / org_height;
    x = std::min(std::max(x, 0), width - 1);
    y = std::min(std::max(y, 0), height - 1);

    for (int y1 = -2; y1 <= 2; ++y1) {
      for (int x1 = -2; x1 <= 2; ++x1) {
        int const kCx = x + x1;
        int const kCy = y + y1;
        if (kCx >= 0 && kCy >= 0 && kCx < width && kCy < height) {
          int const kWeight = (2 * 2) * (2 * 2) - (x1 * y1) * (x1 * y1);
          map[kCy * width + kCx] += v * kWeight;
        }
      }
    }
  }

  int width, height;
  int org_width, org_height;

  std::vector<int> map;
};

void DrawHeatmap(Bitmap& scr, int x, int y, Heatmap& hm);
