#pragma once

#include <string>
#include <vector>

#include "text_cell.hpp"

struct Bitmap;

struct Font {
  struct Char {
    unsigned char data[8 * 7];
    int width;
  };

  Font() : chars(250) {}

  void DrawString(Bitmap& scr, char const* str, std::size_t len, int x, int y, int color, int size);
  int GetDims(char const* str, std::size_t len, int* height = 0);
  void DrawChar(Bitmap& scr, unsigned char ch, int x, int y, int color, int size);

  void DrawChar(Bitmap& scr, unsigned char ch, int x, int y, int color) {
    DrawChar(scr, ch, x, y, color, 1);
  }

  void DrawCenteredText(Bitmap& scr, std::string const& str, int x, int y, int color, int size) {
    int len = GetDims(str) * size;
    DrawString(scr, str.data(), str.size(), x - (len / 2), y, color, size);
  }

  void DrawCenteredText(Bitmap& scr, std::string const& str, int x, int y, int color) {
    DrawCenteredText(scr, str, x, y, color, 1);
  }

  // draws text with a simple shadow underneath it, so even text that would blend into the
  // background can be displayed
  void DrawShadowedText(Bitmap& scr, std::string const& str, int x, int y, int color) {
    DrawString(scr, str, x + 1, y + 1, color / 2);
    DrawString(scr, str, x, y, color);
  }

  void DrawString(Bitmap& scr, char const* str, std::size_t len, int x, int y, int color) {
    DrawString(scr, str, len, x, y, color, 1);
  }

  void DrawString(Bitmap& scr, std::string const& str, int x, int y, int color) {
    DrawString(scr, str.data(), str.size(), x, y, color, 1);
  }

  void DrawString(Bitmap& scr, TextCell const& str, int x, int y, int color) {
    if (str.buffer.empty()) return;

    if (str.placement != TextCell::kLeft) {
      int w = GetDims(reinterpret_cast<char const*>(&str.buffer[0]), str.buffer.size());

      if (str.placement == TextCell::kCenter)
        x -= w / 2;
      else if (str.placement == TextCell::kRight)
        x -= w;
    }

    DrawString(scr, reinterpret_cast<char const*>(&str.buffer[0]), str.buffer.size(), x, y, color,
               1);
  }

  int GetDims(std::string const& str, int* height = 0) {
    return GetDims(str.data(), str.size(), height);
  }

  void DrawFramedText(Bitmap& scr, std::string const& text, int x, int y, int color);

  std::vector<Char> chars;
};
