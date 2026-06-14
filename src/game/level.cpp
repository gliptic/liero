#include "level.hpp"

#include "filesystem.hpp"
#include "game.hpp"
#include "gfx.hpp"
#include "gfx/color.hpp"
#include "io/stream.hpp"

#include <cstring>

void Level::GenerateDirtPattern(Common& common, Rand& rand) {
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
  has_custom_palette = false;

  Resize(settings.random_map_width, settings.random_map_height);
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

  // Retry cap: on small maps the rock formations can fill the level completely,
  // making the do-while unsatisfiable. Use kMaxTries = width * height so large
  // maps are never affected while small ones terminate instead of looping forever.
  int const kMaxTries = width * height;

  count = rand(15) + 5;
  for (int i = 0; i < count; ++i) {
    int cx = 0;
    int cy = 0;
    int tries = 0;
    do {
      cx = rand(width) - 16;

      if (rand(4) == 0) {
        cy = height - 1 - rand(20);
      } else {
        cy = rand(height) - 16;
      }
    } while (!IsNoRock(common, *this, 32, cx, cy) && ++tries < kMaxTries);
    if (tries >= kMaxTries) {
      continue;
    }

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
    int tries = 0;
    do {
      cx = rand(width) - 8;

      if (rand(5) == 0) {
        cy = height - 1 - rand(13);
      } else {
        cy = rand(height) - 8;
      }
    } while (!IsNoRock(common, *this, 15, cx, cy) && ++tries < kMaxTries);
    if (tries >= kMaxTries) {
      continue;
    }

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
  material_id.resize(width * height);
  materials.resize(width * height);
  // Dirty tracking is size-dependent; reset so the next SaveSnapshotFast
  // re-initialises for the new dimensions.
  dirty_bits.clear();
  dirty_list.clear();
}

bool Level::load(Common& common, Settings const& settings, io::Reader& r) {
  // Probe for OLLEVEL2 sized-format header: magic(8) + version(1) + w(2LE) + h(2LE).
  static constexpr uint8_t kSizedMagic[8] = {'O', 'L', 'L', 'E', 'V', 'E', 'L', '2'};
  static constexpr int kMaxDim = 4096;

  uint8_t probe[8] = {};
  std::size_t const kProbeRead = r.TryGet(probe, 8);

  int load_width;
  int load_height;
  uint8_t leftover[8] = {};
  std::size_t leftover_count = 0;

  if (kProbeRead == 8 && std::memcmp(kSizedMagic, probe, 8) == 0) {
    uint8_t hdr[5] = {};
    if (r.TryGet(hdr, 5) != 5) {
      return false;
    }
    load_width = hdr[1] | (static_cast<int>(hdr[2]) << 8);
    load_height = hdr[3] | (static_cast<int>(hdr[4]) << 8);
    if (load_width < 1 || load_width > kMaxDim || load_height < 1 || load_height > kMaxDim) {
      return false;
    }
  } else {
    // Legacy 504×350: the probed bytes are the first kProbeRead material bytes.
    load_width = 504;
    load_height = 350;
    std::memcpy(leftover, probe, kProbeRead);
    leftover_count = kProbeRead;
  }

  Resize(load_width, load_height);
  display_data.clear();
  display_valid.clear();
  argb_ramps.clear();
  display_anim.clear();

  bool reset_palette = true;

  auto* mat_data = reinterpret_cast<uint8_t*>(material_id.data());
  if (leftover_count > 0) {
    std::memcpy(mat_data, leftover, leftover_count);
  }
  r.Get(mat_data + leftover_count,
        static_cast<std::size_t>(load_width) * load_height - leftover_count);

  // Probe buffer for optional extension blocks.  Both "POWERLEVEL" (10 bytes)
  // and "MODERNLV" (8 bytes) may follow the pixel data.  Read the longer
  // probe first so that MODERNLV detection can reuse any unconsumed bytes.
  uint8_t ext_buf[10] = {};
  std::size_t ext_used = 0;

  if (Settings::kExtensions && settings.load_powerlevel_palette) {
    std::size_t const kN = r.TryGet(ext_buf, 10);
    if (kN == 10 && std::memcmp("POWERLEVEL", ext_buf, 10) == 0) {
      Palette pal;
      pal.Read(r);
      origpal.ResetPalette(pal, settings);
      has_custom_palette = true;
      reset_palette = false;
      // ext_buf fully consumed; ext_used stays 0 → MODERNLV probe below reads
      // fresh bytes from the stream.
    } else {
      ext_used = kN;  // bytes pre-read but not matching POWERLEVEL
    }
  }

  // Check for "MODERNLV" block (8-byte magic).
  if (Settings::kExtensions) {
    uint8_t magic_buf[8] = {};
    std::size_t magic_read = 0;

    if (ext_used >= 8) {
      // Pre-read bytes cover the full magic.
      std::memcpy(magic_buf, ext_buf, 8);
      magic_read = 8;
    } else if (ext_used == 0) {
      // No unconsumed bytes (either POWERLEVEL was found/consumed, or the
      // POWERLEVEL check was skipped).  Read fresh bytes for the magic.
      magic_read = r.TryGet(magic_buf, 8);
    }
    // If ext_used in [1,7] the stream was nearly exhausted; no complete block.

    if (magic_read == 8 && std::memcmp("MODERNLV", magic_buf, 8) == 0) {
      std::size_t const kCells = static_cast<std::size_t>(width) * height;
      display_data.resize(kCells);
      display_valid.resize(kCells);

      auto* raw_dd = reinterpret_cast<uint8_t*>(display_data.data());

      // If the POWERLEVEL probe consumed more bytes than the magic (ext_used >
      // 8), those extra bytes are the first bytes of display_data.
      std::size_t prepend = 0;
      if (ext_used > 8) {
        prepend = ext_used - 8;
        std::memcpy(raw_dd, ext_buf + 8, prepend);
      }
      r.Get(raw_dd + prepend, kCells * sizeof(uint32_t) - prepend);
      r.Get(display_valid.data(), kCells);

      // Animation extension: ramp_count(1) + ramps + display_anim(cells).
      // TryGet so files without the anim extension (stream ends here) still load fine.
      uint8_t ramp_count = 0;
      if (r.TryGet(&ramp_count, 1) == 1 && ramp_count > 0) {
        static constexpr uint16_t kMaxColors = 4096;
        bool valid = true;
        std::vector<ArgbRamp> ramps;
        ramps.reserve(ramp_count);

        for (uint8_t ri = 0; ri < ramp_count && valid; ++ri) {
          uint8_t shift_byte = 0;
          if (r.TryGet(&shift_byte, 1) != 1) {
            valid = false;
            break;
          }
          uint8_t count_bytes[2] = {};
          if (r.TryGet(count_bytes, 2) != 2) {
            valid = false;
            break;
          }
          auto const kColorCount = static_cast<uint16_t>(
              static_cast<uint16_t>(count_bytes[0]) | (static_cast<uint16_t>(count_bytes[1]) << 8));
          if (kColorCount == 0 || kColorCount > kMaxColors) {
            valid = false;
            break;
          }
          ArgbRamp ramp;
          ramp.shift = shift_byte;
          ramp.colors.resize(kColorCount);
          auto* raw = reinterpret_cast<uint8_t*>(ramp.colors.data());
          if (r.TryGet(raw, kColorCount * 4U) != kColorCount * 4U) {
            valid = false;
            break;
          }
          ramps.push_back(ramp);
        }

        if (valid) {
          std::vector<uint8_t> anim(kCells);
          if (r.TryGet(anim.data(), kCells) == kCells) {
            for (uint8_t const kRampIdx : anim) {
              if (kRampIdx > ramp_count) {
                valid = false;
                break;
              }
            }
            if (valid) {
              argb_ramps = std::move(ramps);
              display_anim = std::move(anim);
            }
          }
        }
      }
    }
  }

  for (std::size_t i = 0; i < material_id.size(); ++i) {
    materials[i] = common.materials[material_id[i]];
  }

  if (reset_palette) {
    origpal.ResetPalette(common.exepal, settings);
    has_custom_palette = false;
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
  old_random_map_width = settings.random_map_width;
  old_random_map_height = settings.random_map_height;

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

void Level::DrawMiniature(Bitmap& dest, int map_x, int map_y, int step_x, int step_y) const {
  int my = step_y / 2;

  int const kMapEndY = map_y + ((height + step_y / 2) / step_y);
  int const kMapEndX = map_x + ((width + step_x / 2) / step_x);

  for (int y = map_y; y < kMapEndY; ++y) {
    int mx = step_x / 2;
    for (int x = map_x; x < kMapEndX; ++x) {
      auto const kIdx = static_cast<unsigned int>(mx + my * width);
      if (kIdx < material_id.size() && dest.clip_rect.Inside(x, y)) {
        dest.GetPixel(x, y) =
            AppearanceAt(static_cast<int>(kIdx), dest.mode, dest.pal32, dest.cycles);
      }
      mx += step_x;
    }
    my += step_y;
  }
}
