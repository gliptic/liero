#pragma once

#include <cstdint>
#include <cstring>
#include "color.hpp"
#include "math/rect.hpp"

// ARGB8888 screen surface. Drawing primitives take palette indices and
// resolve them at draw time through `pal32`, the owning renderer's
// frame-scope LUT (rebuilt in place every frame — the pointer stays valid).
struct Bitmap {
  int w, h;
  // In pixels, not bytes.
  unsigned int pitch;
  uint32_t* pixels{nullptr};
  // Borrowed from the Renderer that draws into this bitmap (Copy propagates
  // it to capture bitmaps like frozen_screen).
  uint32_t const* pal32{nullptr};
  Rect clip_rect;

  Bitmap() = default;

  Bitmap(const Bitmap&) = delete;
  Bitmap& operator=(const Bitmap&) = delete;

  void Alloc(int w, int h) { Alloc(w, h, w); }

  void Alloc(int new_w, int new_h, unsigned int new_pitch) {
    if (!pixels || w != new_w || h != new_h || pitch != new_pitch) {
      delete[] pixels;
      pixels = new uint32_t[new_pitch * new_h];
      w = new_w;
      h = new_h;
      pitch = new_pitch;
    }

    clip_rect.x1 = 0;
    clip_rect.y1 = 0;
    clip_rect.x2 = w;
    clip_rect.y2 = h;
  }

  uint32_t& GetPixel(int x, int y) const { return (pixels + y * pitch)[x]; }

  void SetPixel(int x, int y, PalIdx v) const {
    if (clip_rect.Inside(x, y)) {
      (pixels + y * pitch)[x] = pal32[v];
    }
  }

  void Copy(Bitmap const& other) {
    Alloc(other.w, other.h, other.pitch);
    pal32 = other.pal32;
    std::memcpy(pixels, other.pixels, sizeof(uint32_t) * other.pitch * other.h);
  }

  ~Bitmap() {
    delete[] pixels;
    pixels = nullptr;
  }
};
