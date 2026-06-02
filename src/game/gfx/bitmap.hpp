#pragma once

#include <cstring>
#include "color.hpp"
#include "math/rect.hpp"

struct Bitmap {
  int w, h;
  unsigned int pitch;
  unsigned char* pixels;
  Rect clip_rect;

  Bitmap() : pixels(0) {}

  Bitmap(const Bitmap&) = delete;
  Bitmap& operator=(const Bitmap&) = delete;

  void Alloc(int w, int h) { Alloc(w, h, w); }

  void Alloc(int new_w, int new_h, unsigned int new_pitch) {
    if (!pixels || w != new_w || h != new_h || pitch != new_pitch) {
      delete[] pixels;
      pixels = new unsigned char[new_pitch * new_h];
      w = new_w;
      h = new_h;
      pitch = new_pitch;
    }

    clip_rect.x1 = 0;
    clip_rect.y1 = 0;
    clip_rect.x2 = w;
    clip_rect.y2 = h;
  }

  unsigned char& GetPixel(int x, int y) {
    return (static_cast<unsigned char*>(pixels) + y * pitch)[x];
  }

  void SetPixel(int x, int y, PalIdx v) {
    if (clip_rect.Inside(x, y)) (static_cast<unsigned char*>(pixels) + y * pitch)[x] = v;
  }

  void Copy(Bitmap const& other) {
    Alloc(other.w, other.h, other.pitch);
    std::memcpy(pixels, other.pixels, other.pitch * other.h);
  }

  ~Bitmap() {
    delete[] pixels;
    pixels = 0;
  }
};
