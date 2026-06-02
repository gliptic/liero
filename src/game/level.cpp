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

  for (int y = 1; y < height; ++y) SetPixel(0, y, ((rand(7) + 12) + Pixel(0, y - 1)) >> 1, common);

  for (int x = 1; x < width; ++x) SetPixel(x, 0, ((rand(7) + 12) + Pixel(x - 1, 0)) >> 1, common);

  for (int y = 1; y < height; ++y)
    for (int x = 1; x < width; ++x) {
      SetPixel(x, y, (Pixel(x - 1, y) + Pixel(x, y - 1) + rand(8) + 12) / 3, common);
    }

  // TODO: Optimize the following

  int count = rand(100);

  for (int i = 0; i < count; ++i) {
    int x = rand(width) - 8;
    int y = rand(height) - 8;

    int temp = rand(4) + 69;

    PalIdx* image = common.large_sprites.SpritePtr(temp);

    for (int cy = 0; cy < 16; ++cy) {
      int my = cy + y;
      if (my >= height) break;

      if (my < 0) continue;

      for (int cx = 0; cx < 16; ++cx) {
        int mx = cx + x;
        if (mx >= width) break;

        if (mx < 0) continue;

        PalIdx src_pix = image[(cy << 4) + cx];
        if (src_pix > 0) {
          PalIdx pix = Pixel(mx, my);
          if (pix > 176 && pix < 180)
            SetPixel(mx, my, (src_pix + pix) / 2, common);
          else
            SetPixel(mx, my, src_pix, common);
        }
      }
    }
  }

  count = rand(15);

  for (int i = 0; i < count; ++i) {
    int x = rand(width) - 8;
    int y = rand(height) - 8;

    int which = rand(4) + 56;

    BlitStone(common, *this, false, common.large_sprites.SpritePtr(which), x, y);
  }
}

bool IsNoRock(Common& common, Level& level, int size, int x, int y) {
  Rect rect(x, y, x + size + 1, y + size + 1);

  rect.Intersect(Rect(0, 0, level.width, level.height));

  for (int y = rect.y1; y < rect.y2; ++y)
    for (int x = rect.x1; x < rect.x2; ++x) {
      if (level.Mat(x, y).Rock()) return false;
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

    int dx = rand(11) - 5;
    int dy = rand(5) - 2;

    int count2 = rand(12);

    for (int j = 0; j < count2; ++j) {
      int count3 = rand(5);

      for (int k = 0; k < count3; ++k) {
        cx += dx;
        cy += dy;
        DrawDirtEffect(common, rand, *this, 1, cx,
                       cy);  // TODO: Check if it really should be dirt effect 1
      }

      cx -= (count3 + 1) * dx;  // TODO: Check if it really should be (count3 + 1)
      cy -= (count3 + 1) * dy;  // TODO: Check if it really should be (count3 + 1)

      cx += rand(7) - 3;
      cy += rand(15) - 7;
    }
  }

  count = rand(15) + 5;
  for (int i = 0; i < count; ++i) {
    int cx, cy;
    do {
      cx = rand(width) - 16;

      if (rand(4) == 0)
        cy = height - 1 - rand(20);
      else
        cy = rand(height) - 16;
    } while (!IsNoRock(common, *this, 32, cx, cy));

    int rock = rand(3);

    BlitStone(common, *this, false, common.large_sprites.SpritePtr(stone_tab[rock][0]), cx, cy);
    BlitStone(common, *this, false, common.large_sprites.SpritePtr(stone_tab[rock][1]), cx + 16,
              cy);
    BlitStone(common, *this, false, common.large_sprites.SpritePtr(stone_tab[rock][2]), cx,
              cy + 16);
    BlitStone(common, *this, false, common.large_sprites.SpritePtr(stone_tab[rock][3]), cx + 16,
              cy + 16);
  }

  count = rand(25) + 5;

  for (int i = 0; i < count; ++i) {
    int cx, cy;
    do {
      cx = rand(width) - 8;

      if (rand(5) == 0)
        cy = height - 1 - rand(13);
      else
        cy = rand(height) - 8;
    } while (!IsNoRock(common, *this, 15, cx, cy));

    BlitStone(common, *this, false, common.large_sprites.SpritePtr(rand(6) + 3), cx, cy);
  }
}

void Level::MakeShadow(Common& common) {
  for (int x = 0; x < width - 3; ++x)
    for (int y = 3; y < height; ++y) {
      if (Mat(x, y).SeeShadow() && Mat(x + 3, y - 3).DirtRock()) {
        SetPixel(x, y, Pixel(x, y) + 4, common);
      }

      if (Pixel(x, y) >= 12 && Pixel(x, y) <= 18 && Mat(x + 3, y - 3).Rock()) {
        SetPixel(x, y, Pixel(x, y) - 2, common);
        if (Pixel(x, y) < 12) SetPixel(x, y, 12, common);
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

  r.Get(reinterpret_cast<uint8_t*>(&data[0]), width * height);

  if (/*len >= 504*350 + 10 + 256*3
   &&*/
      (settings.kExtensions && settings.load_powerlevel_palette)) {
    uint8_t buf[10] = {};
    if (r.TryGet(buf, 10)) {
      if (!std::memcmp("POWERLEVEL", buf, 10)) {
        Palette pal;
        pal.Read(r);
        origpal.ResetPalette(pal, settings);

        reset_palette = false;
      }
    }
  }

  for (std::size_t i = 0; i < data.size(); ++i) materials[i] = common.materials[data[i]];

  if (reset_palette) origpal.ResetPalette(common.exepal, settings);

  return true;
}

void Level::GenerateFromSettings(Common& common, Settings const& settings, Rand& rand) {
  if (settings.random_level) {
    GenerateRandom(common, settings, rand);
  } else {
    std::string path = settings.level_file;
    if (path.find('.', 0) == std::string::npos) path += ".LEV";

    bool loaded = false;
    try {
      auto r_ptr = FsNode(path).ToReader();
      io::Reader& r = *r_ptr;
      loaded = load(common, settings, r);
    } catch (std::runtime_error&) {
      // Ignore
    }

    if (!loaded) GenerateRandom(common, settings, rand);
  }

  old_random_level = settings.random_level;
  old_level_file = settings.level_file;

  if (settings.shadow) {
    MakeShadow(common);
  }
}

using std::vector;

inline bool Free(Material m) { return m.Background() || m.AnyDirt(); }

bool Level::SelectSpawn(Rand& rand, int w, int h, IVec2& selected) {
  vector<int> vruns(width - w + 1);
  vector<int> vdists(width - w + 1);

  Material* m = &materials[0];

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

      int cx = x - (w - 1);
      if (cx < 0) continue;

      int& vrun = vruns[cx];
      int& vdist = vdists[cx];

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
            selected.x = cx;
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

  int map_end_y = map_y + ((height + step / 2) / step);
  int map_end_x = map_x + ((width + step / 2) / step);

  for (int y = map_y; y < map_end_y; ++y) {
    int mx = step / 2;
    for (int x = map_x; x < map_end_x; ++x) {
      dest.GetPixel(x, y) = CheckedPixelWrap(mx, my);
      mx += step;
    }
    my += step;
  }
}
