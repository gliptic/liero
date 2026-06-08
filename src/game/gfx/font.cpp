#include "font.hpp"
#include "../cp437.hpp"
#include "../gfx.hpp"
#include "../reader.hpp"
#include "color.hpp"
#include "macros.hpp"

void Font::DrawChar(Bitmap& scr, unsigned char c, int x, int y, int color, int size) {
  if (c >= 2 && c < 252)  // TODO: Is this correct, shouldn't it be c >= 0 && c < 250, since
                          // drawText subtracts 2?
  {
    uint8_t const* mem = chars[c].data;
    int width = 7;
    int height = 8;
    int const pitch = 7;

    CLIP_IMAGE(scr.clip_rect);

    PalIdx* scrptr = static_cast<PalIdx*>(scr.pixels) + y * scr.pitch + x;

    for (int cy = 0; cy < height; ++cy) {
      for (int i = 0; i < size; i++) {
        PalIdx* rowdest = scrptr;
        PalIdx const* rowsrc = mem;

        for (int cx = 0; cx < width; ++cx) {
          PalIdx const kC = *rowsrc;
          for (int k = 0; k < size; k++) {
            if (kC) {
              *rowdest = color;
            }
            ++rowdest;
          }
          ++rowsrc;
        }

        scrptr += scr.pitch;
      }
      mem += pitch;
    }
  }
}

// Strings reaching the font are UTF-8 (that's what tc.cfg and settings.cfg
// store). The glyph table is CP437 byte-indexed, so we decode each codepoint
// and look it up. Codepoints with no CP437 equivalent are silently skipped —
// drawChar already ignores any byte outside [2, 252), so this stays in sync.
namespace {

unsigned char CodepointToFontByte(char32_t cp) {
  int const kB = cp437::UnicodeToByte(cp);
  return kB < 0 ? 1 : static_cast<unsigned char>(kB);  // 1 = skip-no-draw
}

}  // namespace

void Font::DrawString(Bitmap& scr, char const* str, std::size_t len, int x, int y, int color,
                      int size) {
  int const kOrgX = x;

  for (std::size_t i = 0; i < len;) {
    char32_t const kCp = cp437::Utf8DecodeNext(str, len, i);

    if (kCp == 0) {
      x = kOrgX;
      y += 8 * size;
      continue;
    }

    unsigned char c = CodepointToFontByte(kCp);
    if (c >= 2 && c < 252) {
      c -= 2;

      DrawChar(scr, c, x, y, color, size);

      x += chars[c].width * size;
    }
  }
}

void Font::DrawFramedText(Bitmap& scr, std::string const& text, int x, int y, int color) {
  DrawRoundedBox(scr, x, y, 0, 7, GetDims(text));
  DrawString(scr, text, x + 2, y + 1, color);
}

int Font::GetDims(char const* str, std::size_t len, int* height) {
  int width = 0;
  int max_height = 8;

  int max_width = 0;

  for (std::size_t i = 0; i < len;) {
    char32_t const kCp = cp437::Utf8DecodeNext(str, len, i);

    if (kCp == 0) {
      max_width = std::max(max_width, width);
      width = 0;
      max_height += 8;
      continue;
    }

    unsigned char const kC = CodepointToFontByte(kCp);
    if (kC >= 2 && kC < 252) {
      width += chars[kC - 2].width;
    }
  }

  if (height) {
    *height = max_height;
  }

  return std::max(max_width, width);
}
