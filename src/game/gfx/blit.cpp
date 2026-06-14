#include "blit.hpp"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <map>
#include "../common.hpp"
#include "../constants.hpp"
#include "../level.hpp"
#include "../profiling.hpp"
#include "../rand.hpp"
#include "../settings.hpp"
#include "bitmap.hpp"
#include "macros.hpp"
#include "math/rect.hpp"
#include "shadow_query.hpp"

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
    uint32_t const kArgb = scr.pal32[color];
    for (; y < y2; ++y) {
      uint32_t* row = &scr.GetPixel(x, y);
      std::fill(row, row + (x2 - x), kArgb);
    }
  }
}

void Fill(Bitmap& scr, int color) {
  std::fill(scr.pixels, scr.pixels + scr.pitch * scr.h, scr.pal32[color]);
}

void DrawBar(Bitmap& scr, int x, int y, int width, int color) {
  DrawBar(scr, x, y, width, 2, color);
}

void DrawBar(Bitmap& scr, int x, int y, int width, int height, int color) {
  uint32_t const kArgb = scr.pal32[color];
  for (int h = 0; h < height; h++) {
    if (width > 0) {
      uint32_t* row = &scr.GetPixel(x, y + h);
      std::fill(row, row + width, kArgb);
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
    scr.GetPixel(x, y1) = scr.pal32[color];
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

#define DASH()                                                             \
  do {                                                                     \
    if (scr.clip_rect.Inside(x1, y1) && ((p + phase) % 4) < 2)             \
      scr.GetPixel(x1, y1) = scr.pal32[(p >= color2lim) ? color : color2]; \
    ++p;                                                                   \
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
// BLIT/BLIT3/BLITL, DO_LINE) that hide which locals are mutated and
// which aren't. clang-tidy's const-correctness reasoning across the macro
// boundary produces spurious "can be const" warnings on the parameters and
// macro-injected locals; declaring them const breaks the macros that update
// them. Disable the check for this region.
// NOLINTBEGIN(misc-const-correctness)
// Paints the level's appearance into the screen at (x, y) — the only place
// terrain reaches the screen. AppearanceAt is mode-aware.
void DrawLevel(Bitmap& scr, Level const& level, int x, int y) {
  int width = level.width;
  int height = level.height;
  int const pitch = level.width;
  PalIdx const* mem = level.material_id.data();

  CLIP_IMAGE(scr.clip_rect);

  uint32_t* scrptr = scr.pixels + y * scr.pitch + x;
  int idx = static_cast<int>(mem - level.material_id.data());

  for (int dy = 0; dy < height; ++dy) {
    for (int dx = 0; dx < width; ++dx) {
      scrptr[dx] = level.AppearanceAt(idx + dx, scr.mode, scr.pal32, scr.cycles);
    }

    scrptr += scr.pitch;
    idx += pitch;
  }
}

// ARGB -> ARGB rectangle copy at identical coordinates; the frozen_screen
// restore path. No palette involved.
void BlitBitmap(Bitmap& scr, Bitmap const& src, int x, int y, int width, int height) {
  int const pitch = static_cast<int>(src.pitch);
  uint32_t const* mem = src.pixels + y * pitch + x;

  CLIP_IMAGE(scr.clip_rect);

  uint32_t* scrptr = scr.pixels + y * scr.pitch + x;

  for (int dy = 0; dy < height; ++dy) {
    std::memcpy(scrptr, mem, sizeof(uint32_t) * width);

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

  uint32_t* scrptr = scr.pixels + y * scr.pitch + x;

  for (int y = 0; y < height; ++y) {
    uint32_t* rowdest = scrptr;
    PalIdx const* rowsrc = mem;

    for (int x = 0; x < width; ++x) {
      PalIdx const kC = *rowsrc;
      if (kC) {
        *rowdest = scr.pal32[kC];
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

  uint32_t* scrptr = scr.pixels + y * scr.pitch + x;

  for (int y = 0; y < height; ++y) {
    uint32_t* rowdest = scrptr;
    PalIdx const* rowsrc = mem;

    for (int x = 0; x < width; ++x) {
      PalIdx const kC = *rowsrc;
      if (kC && ((x ^ y ^ phase) & 1)) {
        *rowdest = scr.pal32[kC];
      }
      ++rowsrc;
      ++rowdest;
    }

    scrptr += scr.pitch;
    mem += pitch;
  }
}

// Screen-destination blit driver: `rowdest` is ARGB, bodies resolve palette
// indices through scr.pal32.
#define BLIT(body)                                                                                   \
  do {                                                                                               \
    uint32_t* scrptr = scr.pixels + y * scr.pitch + x;                                               \
    for (int y_ = 0; y_ < height; ++y_) {                                                            \
      uint32_t* rowdest = scrptr;                                                                    \
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

#define BLIT3(body)                                                                               \
  do {                                                                                            \
    uint32_t* scrptr = scr.pixels + y * scr.pitch + x;                                            \
    for (int y_ = 0; y_ < height; ++y_) {                                                         \
      uint32_t* rowdest = scrptr;                                                                 \
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

void BlitImageR(ShadowQuery const& shadow, Bitmap& scr, const PalIdx* mem, int x, int y, int width,
                int height) {
  int const pitch = width;

  CLIP_IMAGE(scr.clip_rect);

  uint32_t* scrptr = scr.pixels + y * scr.pitch + x;

  for (int dy = 0; dy < height; ++dy) {
    uint32_t* rowdest = scrptr;
    PalIdx const* rowsrc = mem;

    for (int dx = 0; dx < width; ++dx) {
      PalIdx const kC = *rowsrc;
      if (kC) {
        // Draw only over the level's special range (classically water);
        // the level, not the screen, is the material source of truth.
        int const kP = shadow.PixelAt(x + dx, y + dy);
        if (kP >= 160 && kP < 168) {
          *rowdest = scr.pal32[kC];
        }
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
        if (c > 116) *rowdest = scr.pal32[c - 5];
      });
      break;

    case 1:
      BLIT({
        if (c > 114) *rowdest = scr.pal32[c - 3];
      });
      break;

    case 2:
      BLIT({
        if (c > 112) *rowdest = scr.pal32[c - 1];
      });
      break;

    default:
      BLIT({
        if (c) *rowdest = scr.pal32[c];
      });
      break;
  }
}

void BlitImageOnMap(Common& common, Level& level, PalIdx* mem, int x, int y, int width,
                    int height) {
  int const pitch = width;
  Rect const kClipRect(0, 0, level.width, level.height);

  CLIP_IMAGE(kClipRect);

  PalIdx const* const kBase = level.material_id.data();
  uint8_t* const kDv = level.display_valid.empty() ? nullptr : level.display_valid.data();
  BLITL(level.material_id.data(), level.width, level.materials.data(), {
    if (c) {
      PalIdx n;
      if (rowmatdest->DirtBack())
        n = c;
      else
        n = c + 3;
      *rowdest = n;
      *rowmatdest = common.materials[n];
      if (kDv) kDv[rowdest - kBase] = 0;
      level.MarkDirty(static_cast<int>(rowdest - kBase));
    }
  });
}

void BlitShadowImage(ShadowQuery const& shadow, Bitmap& scr, const PalIdx* mem, int x, int y,
                     int width, int height) {
  int const pitch = width;

  CLIP_IMAGE(scr.clip_rect);

  uint32_t* scrptr = scr.pixels + y * scr.pitch + x;

  for (int dy = 0; dy < height; ++dy) {
    uint32_t* rowdest = scrptr;
    PalIdx const* rowsrc = mem;

    for (int dx = 0; dx < width; ++dx) {
      PalIdx const kC = *rowsrc;
      if (kC) {
        uint32_t const kShadowed = shadow.ShadowedArgb(x + dx, y + dy);
        if (kShadowed != 0) {
          *rowdest = kShadowed;
        }
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
  PalIdx const* const kBase = level.material_id.data();
  uint8_t* const kDv = level.display_valid.empty() ? nullptr : level.display_valid.data();

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
        if (kDv) {
          kDv[rowdest - kBase] = 0;
        }
        level.MarkDirty(static_cast<int>(rowdest - kBase));
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
          if (kDv) {
            kDv[rowdest - kBase] = 0;
          }
          level.MarkDirty(static_cast<int>(rowdest - kBase));
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

  PalIdx const* const kBase = level.material_id.data();
  uint8_t* const kDv = level.display_valid.empty() ? nullptr : level.display_valid.data();
  if (tex.n_draw_back) {
    BLITL(level.material_id.data(), level.width, level.materials.data(), {
      switch (c) {
        case 6:
          if (rowmatdest->AnyDirt()) {
            int mx = x + x_;
            int my = y + y_;

            *rowdest = t_frame[((my & 15) << 4) + (mx & 15)];
            *rowmatdest = common.materials[*rowdest];
            if (kDv) kDv[rowdest - kBase] = 0;
            level.MarkDirty(static_cast<int>(rowdest - kBase));
          }
          break;

        case 1: {
          Material m = *rowmatdest;
          if (m.Dirt2()) {
            *rowdest = 2;
            *rowmatdest = common.materials[2];
            if (kDv) kDv[rowdest - kBase] = 0;
            level.MarkDirty(static_cast<int>(rowdest - kBase));
          } else if (m.Dirt()) {
            *rowdest = 1;
            *rowmatdest = common.materials[1];
            if (kDv) kDv[rowdest - kBase] = 0;
            level.MarkDirty(static_cast<int>(rowdest - kBase));
          }
        } break;
        default:
          break;
      }
    });
  } else {
    BLITL(level.material_id.data(), level.width, level.materials.data(), {
      switch (c) {
        case 10:
        case 6:
          if (rowmatdest->Background()) {
            int mx = x + x_;
            int my = y + y_;

            *rowdest = t_frame[((my & 15) << 4) + (mx & 15)];
            *rowmatdest = common.materials[*rowdest];
            if (kDv) kDv[rowdest - kBase] = 0;
            level.MarkDirty(static_cast<int>(rowdest - kBase));
          }
          break;

        case 2:
          if (rowmatdest->Background()) {
            *rowdest = 2;
            *rowmatdest = common.materials[2];
            if (kDv) kDv[rowdest - kBase] = 0;
            level.MarkDirty(static_cast<int>(rowdest - kBase));
          }
          break;

        case 1:
          if (rowmatdest->Background()) {
            *rowdest = 1;
            *rowmatdest = common.materials[1];
            if (kDv) kDv[rowdest - kBase] = 0;
            level.MarkDirty(static_cast<int>(rowdest - kBase));
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
  uint32_t* ptr = scr.pixels;
  unsigned int const kPitch = scr.pitch;

  DO_LINE({
    if (++color == LC(NRColourEnd)) color = LC(NRColourBegin);

    if (clip.Inside(cx, cy)) ptr[cy * kPitch + cx] = scr.pal32[color];
  });
}

void DrawLaserSight(Bitmap& scr, Rand& rand, int from_x, int from_y, int to_x, int to_y) {
  Rect const& clip = scr.clip_rect;
  uint32_t* ptr = scr.pixels;
  unsigned int const kPitch = scr.pitch;

  DO_LINE({
    if (rand(5) == 0) {
      if (clip.Inside(cx, cy)) ptr[cy * kPitch + cx] = scr.pal32[rand(2) + 83];
    }
  });
}

void DrawShadowLine(ShadowQuery const& shadow, Bitmap& scr, int from_x, int from_y, int to_x,
                    int to_y) {
  Rect const& clip = scr.clip_rect;
  uint32_t* ptr = scr.pixels;
  unsigned int const kPitch = scr.pitch;

  DO_LINE({
    if (clip.Inside(cx, cy)) {
      uint32_t const kShadowed = shadow.ShadowedArgb(cx, cy);
      if (kShadowed != 0) ptr[cy * kPitch + cx] = kShadowed;
    }
  });
}

void DrawLine(Bitmap& scr, int from_x, int from_y, int to_x, int to_y, int color) {
  Rect const& clip = scr.clip_rect;
  uint32_t* ptr = scr.pixels;
  unsigned int const kPitch = scr.pitch;

  DO_LINE({
    if (clip.Inside(cx, cy)) {
      ptr[cy * kPitch + cx] = scr.pal32[color];
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
    *rowdest = scr.pal32[v];
  });
}

// Per-channel fade at composition time. Identical arithmetic to
// Palette::Fade ((v * amount) >> 5), so fading at composition is
// value-identical to the old palette fade — and it also applies to
// frozen/captured ARGB content, which palette fade used to cover.
static inline uint32_t FadeArgb(uint32_t c, int amount) {
  uint32_t const kR = (((c >> 16) & 0xFFU) * amount) >> 5;
  uint32_t const kG = (((c >> 8) & 0xFFU) * amount) >> 5;
  uint32_t const kB = ((c & 0xFFU) * amount) >> 5;
  return 0xFF000000U | (kR << 16) | (kG << 8) | kB;
}

void ScaleDraw(uint32_t const* src, int w, int h, std::size_t src_pitch, uint8_t* dest,
               std::size_t dest_pitch, int mag, int fade) {
  ZoneScopedN("ScaleDraw");
  bool const kFaded = fade < 32;

  if (mag == 1 && !kFaded) {
    for (int y = 0; y < h; ++y) {
      std::memcpy(dest + y * dest_pitch, src + y * src_pitch, sizeof(uint32_t) * w);
    }
    return;
  }

  for (int y = 0; y < h; ++y) {
    uint32_t const* line = src + y * src_pitch;
    uint8_t* dest_block = dest + y * mag * dest_pitch;

    for (int x = 0; x < w; ++x) {
      uint32_t const kPix = kFaded ? FadeArgb(line[x], fade) : line[x];

      uint8_t* dest_col = dest_block + static_cast<std::size_t>(x) * mag * sizeof(uint32_t);
      for (int dy = 0; dy < mag; ++dy) {
        auto* dest_line = reinterpret_cast<uint32_t*>(dest_col + dy * dest_pitch);
        for (int dx = 0; dx < mag; ++dx) {
          dest_line[dx] = kPix;
        }
      }
    }
  }
}

void ScaleDrawArea(uint32_t const* src, int src_w, int src_h, std::size_t src_pitch, uint32_t* dest,
                   int dest_w, int dest_h, std::size_t dest_pitch) {
  for (int dy = 0; dy < dest_h; ++dy) {
    int const kSy1 = dy * src_h / dest_h;
    int const kSy2 = (dy + 1) * src_h / dest_h;
    for (int dx = 0; dx < dest_w; ++dx) {
      int const kSx1 = dx * src_w / dest_w;
      int const kSx2 = (dx + 1) * src_w / dest_w;
      int const kCount = (kSy2 - kSy1) * (kSx2 - kSx1);
      if (kCount == 0) {
        dest[dy * dest_pitch + dx] = src[kSy1 * src_pitch + kSx1];
        continue;
      }
      uint64_t r = 0;
      uint64_t g = 0;
      uint64_t b = 0;
      for (int sy = kSy1; sy < kSy2; ++sy) {
        uint32_t const* row = src + sy * src_pitch + kSx1;
        for (int sx = kSx1; sx < kSx2; ++sx) {
          uint32_t const kPix = *row++;
          r += (kPix >> 16) & 0xFFU;
          g += (kPix >> 8) & 0xFFU;
          b += kPix & 0xFFU;
        }
      }
      dest[dy * dest_pitch + dx] =
          0xFF000000U | ((r / kCount) << 16) | ((g / kCount) << 8) | (b / kCount);
    }
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