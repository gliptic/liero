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

  void alloc(int w, int h) { alloc(w, h, w); }

  void alloc(int newW, int newH, unsigned int newPitch) {
    if (!pixels || w != newW || h != newH || pitch != newPitch) {
      delete[] pixels;
      pixels = new unsigned char[newPitch * newH];
      w = newW;
      h = newH;
      pitch = newPitch;
    }

    clip_rect.x1 = 0;
    clip_rect.y1 = 0;
    clip_rect.x2 = w;
    clip_rect.y2 = h;
  }

  unsigned char& getPixel(int x, int y) {
    return (static_cast<unsigned char*>(pixels) + y * pitch)[x];
  }

  void setPixel(int x, int y, PalIdx v) {
    if (clip_rect.inside(x, y)) (static_cast<unsigned char*>(pixels) + y * pitch)[x] = v;
  }

  void copy(Bitmap const& other) {
    alloc(other.w, other.h, other.pitch);
    std::memcpy(pixels, other.pixels, other.pitch * other.h);
  }

  ~Bitmap() {
    delete[] pixels;
    pixels = 0;
  }
};
