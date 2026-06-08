#include "level.hpp"

#include "filesystem.hpp"
#include "game.hpp"
#include "gfx.hpp"
#include "gfx/color.hpp"
#include "io/stream.hpp"

#include <cstring>

void Level::GenerateDirtPattern(Common& common, Rand& rand) {
  Resize(504, 350);

  SetPixel(0, 0, rand(7) + 12, common);

  for (int y = 1; y < height; ++y) {
    SetPixel(0, y, ((rand(7) + 12) + Pixel(0, y - 1)) >> 1, common);
  }

  for (int x = 1; x < width; ++x) {
    SetPixel(x, 0, ((rand(7) + 12) + Pixel(x - 1, 0)) >> 1, common);
  }

  for (int y = 1; y < height; ++y) {
    for (int x = 1; x < width; ++x) {
      SetPixel(x, y, (Pixel(x - 1, y) + Pixel(x, y - 1) + rand(8) + 12) / 3, common);
    }
  }

  // TODO: Optimize the following

  int count = rand(100);

  for (int i = 0; i < count; ++i) {
    int const kX = rand(width) - 8;
    int const kY = rand(height) - 8;

    int const kTemp = rand(4) + 69;

    PalIdx const* image = common.large_sprites.SpritePtr(kTemp);

    for (int cy = 0; cy < 16; ++cy) {
      int const kMy = cy + kY;
      if (kMy >= height) {
        break;
      }

      if (kMy < 0) {
        continue;
      }

      for (int cx = 0; cx < 16; ++cx) {
        int const kMx = cx + kX;
        if (kMx >= width) {
          break;
        }

        if (kMx < 0) {
          continue;
        }

        PalIdx const kSrcPix = image[(cy << 4) + cx];
        if (kSrcPix > 0) {
          PalIdx const kPix = Pixel(kMx, kMy);
          if (kPix > 176 && kPix < 180) {
            SetPixel(kMx, kMy, (kSrcPix + kPix) / 2, common);
          } else {
            SetPixel(kMx, kMy, kSrcPix, common);
          }
        }
      }
    }
  }

  count = rand(15);

  for (int i = 0; i < count; ++i) {
    int const kX = rand(width) - 8;
    int const kY = rand(height) - 8;

    int const kWhich = rand(4) + 56;

    BlitStone(common, *this, /*p1=*/false, common.large_sprites.SpritePtr(kWhich), kX, kY);
  }
}

static bool IsNoRock(Common& /*common*/, Level& level, int size, int x, int y) {
  Rect rect(x, y, x + size + 1, y + size + 1);

  rect.Intersect(Rect(0, 0, level.width, level.height));

  for (int y = rect.y1; y < rect.y2; ++y) {
    for (int x = rect.x1; x < rect.x2; ++x) {
      if (level.Mat(x, y).Rock()) {
        return false;
      }
    }
  }

  return true;
}

void Level::GenerateRandom(Common& common, Settings const& settings, Rand& rand) {
  origpal.ResetPalette(common.exepal, settings);

  GenerateDirtPattern(common, rand);

  int count = rand(50) + 5;

  for (int i = 0; i < count; ++i) {
    int cx = rand(width) - 8;
    int cy = rand(height) - 8;

    int const kDx = rand(11) - 5;
    int const kDy = rand(5) - 2;

    int const kCount2 = rand(12);

    for (int j = 0; j < kCount2; ++j) {
      int const kCount3 = rand(5);

      for (int k = 0; k < kCount3; ++k) {
        cx += kDx;
        cy += kDy;
        DrawDirtEffect(common, rand, *this, 1, cx,
                       cy);  // TODO: Check if it really should be dirt effect 1
      }

      cx -= (kCount3 + 1) * kDx;  // TODO: Check if it really should be (count3 + 1)
      cy -= (kCount3 + 1) * kDy;  // TODO: Check if it really should be (count3 + 1)

      cx += rand(7) - 3;
      cy += rand(15) - 7;
    }
  }

  count = rand(15) + 5;
  for (int i = 0; i < count; ++i) {
    int cx = 0;
    int cy = 0;
    do {
      cx = rand(width) - 16;

      if (rand(4) == 0) {
        cy = height - 1 - rand(20);
      } else {
        cy = rand(height) - 16;
      }
    } while (!IsNoRock(common, *this, 32, cx, cy));

    int const kRock = rand(3);

    BlitStone(common, *this, /*p1=*/false, common.large_sprites.SpritePtr(stone_tab[kRock][0]), cx,
              cy);
    BlitStone(common, *this, /*p1=*/false, common.large_sprites.SpritePtr(stone_tab[kRock][1]),
              cx + 16, cy);
    BlitStone(common, *this, /*p1=*/false, common.large_sprites.SpritePtr(stone_tab[kRock][2]), cx,
              cy + 16);
    BlitStone(common, *this, /*p1=*/false, common.large_sprites.SpritePtr(stone_tab[kRock][3]),
              cx + 16, cy + 16);
  }

  count = rand(25) + 5;

  for (int i = 0; i < count; ++i) {
    int cx = 0;
    int cy = 0;
    do {
      cx = rand(width) - 8;

      if (rand(5) == 0) {
        cy = height - 1 - rand(13);
      } else {
        cy = rand(height) - 8;
      }
    } while (!IsNoRock(common, *this, 15, cx, cy));

    BlitStone(common, *this, /*p1=*/false, common.large_sprites.SpritePtr(rand(6) + 3), cx, cy);
  }
}

void Level::MakeShadow(Common& common) {
  for (int x = 0; x < width - 3; ++x) {
    for (int y = 3; y < height; ++y) {
      if (Mat(x, y).SeeShadow() && Mat(x + 3, y - 3).DirtRock()) {
        SetPixel(x, y, Pixel(x, y) + 4, common);
      }

      if (Pixel(x, y) >= 12 && Pixel(x, y) <= 18 && Mat(x + 3, y - 3).Rock()) {
        SetPixel(x, y, Pixel(x, y) - 2, common);
        if (Pixel(x, y) < 12) {
          SetPixel(x, y, 12, common);
        }
      }
    }
  }

  for (int x = 0; x < width; ++x) {
    if (Mat(x, height - 1).Background()) {
      SetPixel(x, height - 1, 13, common);
    }
  }
}

void Level::Resize(int width_new, int height_new) {
  width = width_new;
  height = height_new;
  data.resize(width * height);
  materials.resize(width * height);
}

bool Level::load(Common& common, Settings const& settings, io::Reader& r) {
  Resize(504, 350);

  // std::size_t len = f.len;
  bool reset_palette = true;

  r.Get(reinterpret_cast<uint8_t*>(data.data()), width * height);

  if (/*len >= 504*350 + 10 + 256*3
   &&*/
      (Settings::kExtensions && settings.load_powerlevel_palette)) {
    uint8_t buf[10] = {};
    if (r.TryGet(buf, 10)) {
      if (std::memcmp("POWERLEVEL", buf, 10) == 0) {
        Palette pal;
        pal.Read(r);
        origpal.ResetPalette(pal, settings);

        reset_palette = false;
      }
    }
  }

  for (std::size_t i = 0; i < data.size(); ++i) {
    materials[i] = common.materials[data[i]];
  }

  if (reset_palette) {
    origpal.ResetPalette(common.exepal, settings);
  }

  return true;
}

void Level::GenerateFromSettings(Common& common, Settings const& settings, Rand& rand) {
  if (settings.random_level) {
    GenerateRandom(common, settings, rand);
  } else {
    std::string path = settings.level_file;
    if (!path.contains('.')) {
      path += ".LEV";
    }

    bool loaded = false;
    try {
      auto r_ptr = FsNode(path).ToReader();
      io::Reader& r = *r_ptr;
      loaded = load(common, settings, r);
    } catch (std::runtime_error&) {  // NOLINT(bugprone-empty-catch) — `loaded` stays false and we
                                     // fall through to GenerateRandom.
      // Ignore
    }

    if (!loaded) {
      GenerateRandom(common, settings, rand);
    }
  }

  old_random_level = settings.random_level;
  old_level_file = settings.level_file;

  if (settings.shadow) {
    MakeShadow(common);
  }
}

using std::vector;

static inline bool Free(Material m) { return m.Background() || m.AnyDirt(); }

bool Level::SelectSpawn(Rand& rand, int w, int h, IVec2& selected) {
  vector<int> vruns(width - w + 1);
  vector<int> vdists(width - w + 1);

  Material const* m = materials.data();

  uint32_t i = 0;

  for (int y = 0; y < height; ++y) {
    int hrun = 0;
    int filled = 0;

    for (int x = 0; x < width; ++x) {
      if (Free(*m)) {
        ++hrun;
      } else {
        hrun = 0;
        ++filled;
      }
      ++m;

      int const kCx = x - (w - 1);
      if (kCx < 0) {
        continue;
      }

      int& vrun = vruns[kCx];
      int& vdist = vdists[kCx];

      if (hrun >= w) {
        if (vdist > 0) {
          vrun = 0;
          vdist = 0;
        }
        ++vrun;
      } else {
        if (vrun >= h && vdist <= 8 && filled > w / 4) {
          // We have a supported square at (x + 1 - w, y - h)
          ++i;
          if (rand(i) < 1) {
            selected.x = kCx;
            selected.y = y - h;
          }
        }
        ++vdist;
      }

      filled -= !Free(m[-w]);
    }
  }

  return i > 0;
}

void Level::DrawMiniature(Bitmap& dest, int map_x, int map_y, int step) {
  int my = step / 2;

  int const kMapEndY = map_y + ((height + step / 2) / step);
  int const kMapEndX = map_x + ((width + step / 2) / step);

  for (int y = map_y; y < kMapEndY; ++y) {
    int mx = step / 2;
    for (int x = map_x; x < kMapEndX; ++x) {
      dest.GetPixel(x, y) = CheckedPixelWrap(mx, my);
      mx += step;
    }
    my += step;
  }
}
