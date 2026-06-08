#include "blit.hpp"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <map>
#include "../common.hpp"
#include "../constants.hpp"
#include "../level.hpp"
#include "../rand.hpp"
#include "../settings.hpp"
#include "bitmap.hpp"
#include "macros.hpp"
#include "math/rect.hpp"

void FillRect(Bitmap& scr, int x, int y, int w, int h, int color) {
  int x2 = x + w;
  int y2 = y + h;
  int const kClipx2 = scr.clip_rect.x2;
  int const kClipy2 = scr.clip_rect.y2;
  x = std::max(x, static_cast<int>(scr.clip_rect.x1));
  y = std::max(y, static_cast<int>(scr.clip_rect.y1));
  x2 = std::min(x2, kClipx2);
  y2 = std::min(y2, kClipy2);

  if (x2 > x) {
    for (; y < y2; ++y) {
      std::memset(&scr.GetPixel(x, y), color, x2 - x);
    }
  }
}

void Fill(Bitmap& scr, int color) { std::memset(scr.pixels, color, scr.pitch * scr.h); }

void DrawBar(Bitmap& scr, int x, int y, int width, int color) {
  DrawBar(scr, x, y, width, 2, color);
}

void DrawBar(Bitmap& scr, int x, int y, int width, int height, int color) {
  for (int h = 0; h < height; h++) {
    if (width > 0) {
      std::memset(&scr.GetPixel(x, y + h), color, width);
    }
  }
}

void Vline(Bitmap& scr, int x, int y1, int y2, int color) {
  if (x < scr.clip_rect.x1 || x >= scr.clip_rect.x2) {
    return;
  }

  y1 = std::max(y1, static_cast<int>(scr.clip_rect.y1));
  y2 = std::min(y2, static_cast<int>(scr.clip_rect.y2));

  for (; y1 < y2; ++y1) {
    scr.GetPixel(x, y1) = color;
  }
}

void DrawRoundedBox(Bitmap& scr, int x, int y, int color, int height, int width) {
  FillRect(scr, x, y + 1, width + 3, height - 2, color);
  FillRect(scr, x + 1, y, width + 1, 1, color);
  FillRect(scr, x + 1, y + height - 1, width + 1, 1, color);
  /*
  height--;
  std::memset(&scr.GetPixel(x+1,y), color, width+1);
  for(long i=1; i<height; i++)
  {
          std::memset(&scr.GetPixel(x,y+i), color, width+3);
  }
  std::memset(&scr.GetPixel(x+1,y+height), color, width+1);*/
}

void DrawRoundedLineBox(Bitmap& scr, int x, int y, int color, int width, int height) {
  FillRect(scr, x + 1, y, width - 2, 1, color);
  FillRect(scr, x + 1, y + height - 1, width - 2, 1, color);
  FillRect(scr, x, y + 1, 1, height - 2, color);
  FillRect(scr, x + width - 1, y + 1, 1, height - 2, color);
}

#define DASH()                                                  \
  do {                                                          \
    if (scr.clip_rect.Inside(x1, y1) && ((p + phase) % 4) < 2)  \
      scr.GetPixel(x1, y1) = (p >= color2lim) ? color : color2; \
    ++p;                                                        \
  } while (0)

void DrawDashedLineBox(Bitmap& scr, int x, int y, int color, int color2, int num, int den,
                       int width, int height, int phase) {
  int p = 0;
  int x1 = 0;
  int y1 = 0;
  int const kPerim = 2 * (width + height) - 2;
  int const color2lim = num * kPerim / den;

  x1 = x;
  for (y1 = y; y1 < y + height; ++y1) {
    DASH();
  }

  y1 = y + height - 1;
  for (x1 = x + 1; x1 < x + width - 1; ++x1) {
    DASH();
  }

  x1 = x + width - 1;
  for (y1 = y + height - 1; y1 >= y; --y1) {
    DASH();
  }

  y1 = y;
  for (x1 = x + width - 2; x1 > x; --x1) {
    DASH();
  }
}

// The blitter routines below are driven by macros (CLIP_IMAGE, UNPACK_SPRITE,
// BLIT/BLIT2/BLIT3/BLITL, DO_LINE) that hide which locals are mutated and
// which aren't. clang-tidy's const-correctness reasoning across the macro
// boundary produces spurious "can be const" warnings on the parameters and
// macro-injected locals; declaring them const breaks the macros that update
// them. Disable the check for this region.
// NOLINTBEGIN(misc-const-correctness)
void BlitImageNoKeyColour(Bitmap& scr, PalIdx* mem, int x, int y, int width, int height,
                          int pitch) {
  CLIP_IMAGE(scr.clip_rect);

  PalIdx* scrptr = static_cast<PalIdx*>(scr.pixels) + y * scr.pitch + x;

  for (int y = 0; y < height; ++y) {
    std::memcpy(scrptr, mem, width);

    scrptr += scr.pitch;
    mem += pitch;
  }
}

#define UNPACK_SPRITE(s)   \
  int pitch = (s).pitch;   \
  int width = (s).width;   \
  int height = (s).height; \
  PalIdx* mem = (s).mem

void BlitImage(Bitmap& scr, Sprite spr, int x, int y) {
  UNPACK_SPRITE(spr);

  CLIP_IMAGE(scr.clip_rect);

  PalIdx* scrptr = static_cast<PalIdx*>(scr.pixels) + y * scr.pitch + x;

  for (int y = 0; y < height; ++y) {
    PalIdx* rowdest = scrptr;
    PalIdx const* rowsrc = mem;

    for (int x = 0; x < width; ++x) {
      PalIdx const kC = *rowsrc;
      if (kC) {
        *rowdest = kC;
      }
      ++rowsrc;
      ++rowdest;
    }

    scrptr += scr.pitch;
    mem += pitch;
  }
}

void BlitImageTrans(Bitmap& scr, Sprite spr, int x, int y, int phase) {
  UNPACK_SPRITE(spr);

  CLIP_IMAGE(scr.clip_rect);

  PalIdx* scrptr = static_cast<PalIdx*>(scr.pixels) + y * scr.pitch + x;

  for (int y = 0; y < height; ++y) {
    PalIdx* rowdest = scrptr;
    PalIdx const* rowsrc = mem;

    for (int x = 0; x < width; ++x) {
      PalIdx const kC = *rowsrc;
      if (kC && ((x ^ y ^ phase) & 1)) {
        *rowdest = kC;
      }
      ++rowsrc;
      ++rowdest;
    }

    scrptr += scr.pitch;
    mem += pitch;
  }
}

#define BLIT(body)                                                                                   \
  do {                                                                                               \
    PalIdx* scrptr = static_cast<PalIdx*>(scr.pixels) + y * scr.pitch + x;                           \
    for (int y_ = 0; y_ < height; ++y_) {                                                            \
      PalIdx* rowdest = scrptr;                                                                      \
      PalIdx* rowsrc = mem;                                                                          \
      for (int x_ = 0; x_ < width; ++x_) {                                                           \
        PalIdx c = *rowsrc;                                                                          \
        body++ rowsrc; /* NOLINT(bugprone-macro-parentheses) — body expands to a statement, not an \
                          expression */                                                              \
        ++rowdest;                                                                                   \
      }                                                                                              \
      scrptr += scr.pitch;                                                                           \
      mem += pitch;                                                                                  \
    }                                                                                                \
  } while (false)

#define BLIT2(pixels, destpitch, body)                                                               \
  do {                                                                                               \
    PalIdx* scrptr = static_cast<PalIdx*>(pixels) + y * (destpitch) + x;                             \
    for (int y_ = 0; y_ < height; ++y_) {                                                            \
      PalIdx* rowdest = scrptr;                                                                      \
      PalIdx* rowsrc = mem;                                                                          \
      for (int x_ = 0; x_ < width; ++x_) {                                                           \
        PalIdx c = *rowsrc;                                                                          \
        body++ rowsrc; /* NOLINT(bugprone-macro-parentheses) — body expands to a statement, not an \
                          expression */                                                              \
        ++rowdest;                                                                                   \
      }                                                                                              \
      scrptr += (destpitch);                                                                         \
      mem += pitch;                                                                                  \
    }                                                                                                \
  } while (false)

#define BLIT3(body)                                                                               \
  do {                                                                                            \
    PalIdx* scrptr = static_cast<PalIdx*>(scr.pixels) + y * scr.pitch + x;                        \
    for (int y_ = 0; y_ < height; ++y_) {                                                         \
      PalIdx* rowdest = scrptr;                                                                   \
      for (int x_ = 0; x_ < width; ++x_) {                                                        \
        body++ mem; /* NOLINT(bugprone-macro-parentheses) — body expands to a statement, not an \
                       expression */                                                              \
        ++rowdest;                                                                                \
      }                                                                                           \
      scrptr += scr.pitch;                                                                        \
      mem += pitch - width;                                                                       \
    }                                                                                             \
  } while (false)

#define BLITL(pixels, destpitch, matpixels, body)                                                    \
  do {                                                                                               \
    PalIdx* scrptr = (pixels) + y * (destpitch) + x;                                                 \
    Material* matptr = (matpixels) + y * (destpitch) + x;                                            \
    for (int y_ = 0; y_ < height; ++y_) {                                                            \
      PalIdx* rowdest = scrptr;                                                                      \
      Material* rowmatdest = matptr;                                                                 \
      PalIdx* rowsrc = mem;                                                                          \
      for (int x_ = 0; x_ < width; ++x_) {                                                           \
        PalIdx c = *rowsrc;                                                                          \
        body++ rowsrc; /* NOLINT(bugprone-macro-parentheses) — body expands to a statement, not an \
                          expression */                                                              \
        ++rowdest;                                                                                   \
        ++rowmatdest;                                                                                \
      }                                                                                              \
      scrptr += (destpitch);                                                                         \
      matptr += (destpitch);                                                                         \
      mem += pitch;                                                                                  \
    }                                                                                                \
  } while (false)

void BlitImageR(Bitmap& scr, const PalIdx* mem, int x, int y, int width, int height) {
  int const pitch = width;

  CLIP_IMAGE(scr.clip_rect);

  PalIdx* scrptr = static_cast<PalIdx*>(scr.pixels) + y * scr.pitch + x;

  for (int y = 0; y < height; ++y) {
    PalIdx* rowdest = scrptr;
    PalIdx const* rowsrc = mem;

    for (int x = 0; x < width; ++x) {
      PalIdx const kC = *rowsrc;
      if (kC && (static_cast<PalIdx>(*rowdest - 160) < 8)) {
        *rowdest = kC;
      }
      ++rowsrc;
      ++rowdest;
    }

    scrptr += scr.pitch;
    mem += pitch;
  }
}

void BlitFireCone(Bitmap& scr, int fc, PalIdx* mem, int x, int y) {
  int width = 16;
  int height = 16;
  int const pitch = width;

  CLIP_IMAGE(scr.clip_rect);

  switch (fc) {
    case 0:
      BLIT({
        if (c > 116) *rowdest = c - 5;
      });
      break;

    case 1:
      BLIT({
        if (c > 114) *rowdest = c - 3;
      });
      break;

    case 2:
      BLIT({
        if (c > 112) *rowdest = c - 1;
      });
      break;

    default:
      BLIT({
        if (c) *rowdest = c;
      });
      break;
  }
}

void BlitImageOnMap(Common& common, Level& level, PalIdx* mem, int x, int y, int width,
                    int height) {
  int const pitch = width;
  Rect const kClipRect(0, 0, level.width, level.height);

  CLIP_IMAGE(kClipRect);

  BLITL(level.data.data(), level.width, level.materials.data(), {
    if (c) {
      PalIdx n;
      if (rowmatdest->DirtBack())
        n = c;
      else
        n = c + 3;
      *rowdest = n;
      *rowmatdest = common.materials[n];
    }
  });
}

void BlitShadowImage(Common& common, Bitmap& scr, const PalIdx* mem, int x, int y, int width,
                     int height) {
  int const pitch = width;

  CLIP_IMAGE(scr.clip_rect);

  PalIdx* scrptr = static_cast<PalIdx*>(scr.pixels) + y * scr.pitch + x;

  for (int y = 0; y < height; ++y) {
    PalIdx* rowdest = scrptr;
    PalIdx const* rowsrc = mem;

    for (int x = 0; x < width; ++x) {
      PalIdx const kC = *rowsrc;
      if (kC && common.materials[*rowdest].SeeShadow()) {  // TODO: Speed up this test?
        *rowdest += 4;
      }
      ++rowsrc;
      ++rowdest;
    }

    scrptr += scr.pitch;
    mem += pitch;
  }
}

void BlitStone(Common& common, Level& level, bool p1, const PalIdx* mem, int x, int y) {
  int width = 16;
  int height = 16;
  int const pitch = width;

  Rect const kClip(0, 0, level.width, level.height);

  CLIP_IMAGE(kClip);

  PalIdx* dest = level.Pixelp(x, y);
  Material* matdest = level.Matp(x, y);

  if (p1) {
    for (int y = 0; y < height; ++y) {
      PalIdx* rowdest = dest;
      Material* rowmatdest = matdest;
      PalIdx const* rowsrc = mem;

      for (int x = 0; x < width; ++x) {
        PalIdx const kC = *rowsrc;
        PalIdx n = 0;
        if (kC && rowmatdest->DirtBack()) {  // TODO: Speed up this test?
          n = kC;
        } else {
          n = kC + 3;
        }
        *rowdest = n;
        *rowmatdest = common.materials[n];
        ++rowsrc;
        ++rowdest;
        ++rowmatdest;
      }

      dest += level.width;
      matdest += level.width;
      mem += pitch;
    }
  } else {
    for (int y = 0; y < height; ++y) {
      PalIdx* rowdest = dest;
      Material* rowmatdest = matdest;
      PalIdx const* rowsrc = mem;

      for (int x = 0; x < width; ++x) {
        PalIdx const kC = *rowsrc;
        if (kC) {
          *rowdest = kC;
          *rowmatdest = common.materials[kC];
        }

        ++rowsrc;
        ++rowdest;
        ++rowmatdest;
      }

      dest += level.width;
      matdest += level.width;
      mem += pitch;
    }
  }
}

void DrawDirtEffect(Common& common, Rand& rand, Level& level, int dirt_effect, int x, int y) {
  assert(dirt_effect >= 0 && dirt_effect < 9);
  Texture const& tex = common.textures[dirt_effect];
  PalIdx const* t_frame = common.large_sprites.SpritePtr(tex.s_frame + rand(tex.r_frame));
  PalIdx* m_frame = common.large_sprites.SpritePtr(tex.m_frame);

  int width = 16;
  int height = 16;
  int const pitch = width;
  PalIdx* mem = m_frame;

  Rect const kClip(0, 0, level.width, level.height - 1);

  CLIP_IMAGE(kClip);

  if (tex.n_draw_back) {
    BLITL(level.data.data(), level.width, level.materials.data(), {
      switch (c) {
        case 6:
          if (rowmatdest->AnyDirt()) {
            int mx = x + x_;
            int my = y + y_;

            *rowdest = t_frame[((my & 15) << 4) + (mx & 15)];
            *rowmatdest = common.materials[*rowdest];
          }
          break;

        case 1: {
          Material m = *rowmatdest;
          if (m.Dirt2()) {
            *rowdest = 2;
            *rowmatdest = common.materials[2];
          } else if (m.Dirt()) {
            *rowdest = 1;
            *rowmatdest = common.materials[1];
          }
        } break;
        default:
          break;
      }
    });
  } else {
    BLITL(level.data.data(), level.width, level.materials.data(), {
      switch (c) {
        case 10:
        case 6:
          if (rowmatdest->Background()) {
            int mx = x + x_;
            int my = y + y_;

            *rowdest = t_frame[((my & 15) << 4) + (mx & 15)];
            *rowmatdest = common.materials[*rowdest];
          }
          break;

        case 2:
          if (rowmatdest->Background()) {
            *rowdest = 2;
            *rowmatdest = common.materials[2];
          }
          break;

        case 1:
          if (rowmatdest->Background()) {
            *rowdest = 1;
            *rowmatdest = common.materials[1];
          }
          break;
        default:
          break;
      }
    });
  }
}

void CorrectShadow(Common& common, Level& level, Rect rect) {
  rect.Intersect(Rect(0, 3, level.width - 3, level.height));

  for (int x = rect.x1; x < rect.x2; ++x) {
    for (int y = rect.y1; y < rect.y2; ++y) {
      PalIdx const kPix = level.Pixel(x, y);

      if (level.Mat(x, y).SeeShadow() && level.Mat(x + 3, y - 3).DirtRock()) {
        level.SetPixel(x, y, kPix + 4, common);
      } else if (kPix >= 164  // Remove shadow
                 && kPix <= 167 && !level.Mat(x + 3, y - 3).DirtRock()) {
        level.SetPixel(x, y, kPix - 4, common);
      }
    }
  }
}

// NOLINTNEXTLINE(readability-avoid-nested-conditional-operator) — standard sign helper; nesting matches the math.
static inline int sign(int v) { return v < 0 ? -1 : (v > 0 ? 1 : 0); }

#define DO_LINE(body_)                                                                \
  {                                                                                   \
    int cx = from_x;                                                                  \
    int cy = from_y;                                                                  \
    int dx = to_x - from_x;                                                           \
    int dy = to_y - from_y;                                                           \
    int sx = sign(dx);                                                                \
    int sy = sign(dy);                                                                \
    dx = std::abs(dx);                                                                \
    dy = std::abs(dy);                                                                \
    if (dx > dy) {                                                                    \
      int c = -(dx >> 1);                                                             \
      while (cx != to_x) {                                                            \
        c += dy;                                                                      \
        cx += sx;                                                                     \
        if (c > 0) {                                                                  \
          cy += sy;                                                                   \
          c -= dx;                                                                    \
        }                                                                             \
        body_ /* NOLINT(bugprone-macro-parentheses) — body_ expands to a statement */ \
      }                                                                               \
    } else {                                                                          \
      int c = -(dy >> 1);                                                             \
      while (cy != to_y) {                                                            \
        c += dx;                                                                      \
        cy += sy;                                                                     \
        if (c > 0) {                                                                  \
          cx += sx;                                                                   \
          c -= dy;                                                                    \
        }                                                                             \
        body_ /* NOLINT(bugprone-macro-parentheses) — body_ expands to a statement */ \
      }                                                                               \
    }                                                                                 \
  }

void DrawNinjarope(Common& common, Bitmap& scr, int from_x, int from_y, int to_x, int to_y) {
  int color = LC(NRColourBegin);

  Rect const& clip = scr.clip_rect;
  PalIdx* ptr = scr.pixels;
  unsigned int const kPitch = scr.pitch;

  DO_LINE({
    if (++color == LC(NRColourEnd)) color = LC(NRColourBegin);

    if (clip.Inside(cx, cy)) ptr[cy * kPitch + cx] = color;
  });
}

void DrawLaserSight(Bitmap& scr, Rand& rand, int from_x, int from_y, int to_x, int to_y) {
  Rect const& clip = scr.clip_rect;
  PalIdx* ptr = scr.pixels;
  unsigned int const kPitch = scr.pitch;

  DO_LINE({
    if (rand(5) == 0) {
      if (clip.Inside(cx, cy)) ptr[cy * kPitch + cx] = rand(2) + 83;
    }
  });
}

void DrawShadowLine(Common& common, Bitmap& scr, int from_x, int from_y, int to_x, int to_y) {
  Rect const& clip = scr.clip_rect;
  PalIdx* ptr = scr.pixels;
  unsigned int const kPitch = scr.pitch;

  DO_LINE({
    if (clip.Inside(cx, cy)) {
      PalIdx& pix = ptr[cy * kPitch + cx];
      if (common.materials[pix].SeeShadow()) pix += 4;
    }
  });
}

void DrawLine(Bitmap& scr, int from_x, int from_y, int to_x, int to_y, int color) {
  Rect const& clip = scr.clip_rect;
  PalIdx* ptr = scr.pixels;
  unsigned int const kPitch = scr.pitch;

  DO_LINE({
    if (clip.Inside(cx, cy)) {
      ptr[cy * kPitch + cx] = color;
    }
  });
}

void DrawGraph(Bitmap& scr, std::vector<double> const& data, int height, int start_x, int start_y,
               int color, int neg_color, bool balanced) {
  if (!data.empty()) {
    int x = start_x;

    int const kBaseY = start_y + (balanced ? height / 2 : height);

    for (double const kV : data) {
      int y1 = kBaseY - static_cast<int>(std::floor(kV + 0.5));
      int y2 = kBaseY;
      if (y1 > y2) {
        std::swap(y1, y2);
      }
      Vline(scr, x, y1, y2, kV >= 0 ? color : neg_color);
      ++x;
    }
  }

  DrawRoundedLineBox(scr, start_x, start_y, 7, static_cast<int>(data.size()), height);
}

void DrawHeatmap(Bitmap& scr, int x, int y, Heatmap& hm) {
  int width = hm.width;
  int height = hm.height;
  int const pitch = width;
  int const* mem = hm.map.data();

  std::map<int, int> counts;
  int const* p = mem;
  int total_pixels = 0;
  while (p != mem + width * height) {
    if (*p != 0) {
      ++counts[*p];
      ++total_pixels;
    }
    ++p;
  }

  std::map<int, int> mapping;
  int cum = 0;
  int const kMaxIdx = 119 - 104 + 1;

  mapping[0] = 0;

  for (auto& v : counts) {
    mapping[v.first] = static_cast<int>(104 + static_cast<int64_t>(cum) * kMaxIdx / total_pixels);
    cum += v.second;
  }

  CLIP_IMAGE(scr.clip_rect);

  BLIT3({
    int v = mapping[*mem];
    *rowdest = v;
  });
}

void ScaleDraw(PalIdx* src, int w, int h, std::size_t src_pitch, uint8_t* dest,
               std::size_t dest_pitch, int mag, const uint32_t* pal32) {
  if (mag == 1) {
    for (int y = 0; y < h; ++y) {
      PalIdx const* line = src + y * src_pitch;
      auto* dest_line = reinterpret_cast<uint32_t*>(dest + y * dest_pitch);

      for (int x = 0; x < w; ++x) {
        PalIdx const kPix = *line++;
        *dest_line++ = pal32[kPix];
      }
    }
  } else if (mag > 1) {
    for (int y = 0; y < h; ++y) {
      PalIdx* line = src + y * src_pitch;
      int const kDestMagPitch = mag * static_cast<int>(dest_pitch);
      uint8_t* dest_line = dest + y * kDestMagPitch;

      for (int x = 0; x < w / 4; ++x) {
        uint32_t const kPix = *reinterpret_cast<uint32_t*>(line);
        line += 4;

        uint32_t const kA = pal32[kPix >> 24];
        uint32_t const kB = pal32[(kPix & 0x00ff0000) >> 16];
        uint32_t const kC = pal32[(kPix & 0x0000ff00) >> 8];
        uint32_t const kD = pal32[kPix & 0x000000ff];

        for (int dx = 0; dx < mag; ++dx) {
          for (int dy = 0; dy < kDestMagPitch; dy += static_cast<int>(dest_pitch)) {
            *reinterpret_cast<uint32_t*>(dest_line + dy) = kD;
          }
          dest_line += 4;
        }
        for (int dx = 0; dx < mag; ++dx) {
          for (int dy = 0; dy < kDestMagPitch; dy += static_cast<int>(dest_pitch)) {
            *reinterpret_cast<uint32_t*>(dest_line + dy) = kC;
          }
          dest_line += 4;
        }
        for (int dx = 0; dx < mag; ++dx) {
          for (int dy = 0; dy < kDestMagPitch; dy += static_cast<int>(dest_pitch)) {
            *reinterpret_cast<uint32_t*>(dest_line + dy) = kB;
          }
          dest_line += 4;
        }
        for (int dx = 0; dx < mag; ++dx) {
          for (int dy = 0; dy < kDestMagPitch; dy += static_cast<int>(dest_pitch)) {
            *reinterpret_cast<uint32_t*>(dest_line + dy) = kA;
          }
          dest_line += 4;
        }
      }
    }
  }
}

void PreparePaletteBgra(Color real_pal[256], uint32_t (&pal32)[256]) {
  for (int i = 0; i < 256; ++i) {
    pal32[i] = (real_pal[i].r << 16) | (real_pal[i].g << 8) | real_pal[i].b;
  }
}

int FitScreen(int back_w, int back_h, int scr_w, int scr_h, int& offset_x, int& offset_y) {
  int mag = 1;

  while (scr_w * mag <= back_w && scr_h * mag <= back_h) {
    ++mag;
  }

  --mag;  // mag was the first that didn't fit

  scr_w *= mag;
  scr_h *= mag;

  offset_x = back_w / 2 - scr_w / 2;
  offset_y = back_h / 2 - scr_h / 2;

  return mag;
}
// NOLINTEND(misc-const-correctness)