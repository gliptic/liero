#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

#include "game/gfx/palette.hpp"
#include "game/io/stream.hpp"
#include "game/level.hpp"
#include "game/worm.hpp"

namespace {

// Golden reference for the VGA-era pipeline: palette files carry 6-bit
// channels which reach the screen as (v & 63) << 2. The 8-bit palette
// refactor must keep steady-state output byte-identical to this.
uint8_t Legacy6BitToScreen(uint8_t raw) { return static_cast<uint8_t>((raw & 63) << 2); }

// Golden reference for the worm colour ramp: the 0..255 input is quantized
// to the VGA grid, the ramp computed in 6-bit and expanded to screen range.
uint8_t LegacyScaleAddToScreen(int c, int scale, int add) {
  return static_cast<uint8_t>(((add + (c >> 2) * scale) / 64) << 2);
}

struct Rgb {
  uint8_t r, g, b;
};

// The hand-tuned 5-entry gradient from Palette::SetWormColoursSpan,
// evaluated with the legacy 6-bit math.
std::vector<Rgb> LegacyWormRamp(int const (&rgb)[3]) {
  struct {
    int scale, add;
  } const kSteps[5] = {{.scale = 38, .add = 0},
                       {.scale = 50, .add = 0},
                       {.scale = 64, .add = 0},
                       {.scale = 47, .add = 1008},
                       {.scale = 28, .add = 2205}};

  std::vector<Rgb> ramp;
  for (auto const& step : kSteps) {
    ramp.push_back({.r = LegacyScaleAddToScreen(rgb[0], step.scale, step.add),
                    .g = LegacyScaleAddToScreen(rgb[1], step.scale, step.add),
                    .b = LegacyScaleAddToScreen(rgb[2], step.scale, step.add)});
  }
  return ramp;
}

void RequireEntry(Color const& actual, Rgb const& expected) {
  REQUIRE(static_cast<int>(actual.r) == static_cast<int>(expected.r));
  REQUIRE(static_cast<int>(actual.g) == static_cast<int>(expected.g));
  REQUIRE(static_cast<int>(actual.b) == static_cast<int>(expected.b));
}

}  // namespace

TEST_CASE("classic palette load and activate matches the VGA pipeline", "[palette]") {
  // Cover the full byte range, including values above 63 that legacy
  // palette files would have had masked off.
  std::vector<uint8_t> raw(256 * 3);
  for (std::size_t i = 0; i < raw.size(); ++i) {
    raw[i] = static_cast<uint8_t>((i * 7 + 3) & 0xff);
  }

  io::MemReader r(raw.data(), raw.size());
  Palette pal;
  pal.Read(r);

  Color real_pal[256];
  pal.Activate(real_pal);

  for (int i = 0; i < 256; ++i) {
    REQUIRE(static_cast<int>(real_pal[i].r) == static_cast<int>(Legacy6BitToScreen(raw[i * 3])));
    REQUIRE(static_cast<int>(real_pal[i].g) ==
            static_cast<int>(Legacy6BitToScreen(raw[i * 3 + 1])));
    REQUIRE(static_cast<int>(real_pal[i].b) ==
            static_cast<int>(Legacy6BitToScreen(raw[i * 3 + 2])));
  }
}

TEST_CASE("worm colour ramps match the legacy shading", "[palette]") {
  // Default worm colours from WormSettings / settings.cpp (the legacy 6-bit
  // defaults expanded by << 2).
  int const kWormRgb[2][3] = {{104, 104, 252}, {60, 172, 60}};
  // Sprite ramp bases and secondary copy locations for each worm index.
  int const kBase[2] = {32, 41};
  int const kColourIndex[2] = {0x58, 0x78};

  for (int w = 0; w < 2; ++w) {
    WormSettings ws;
    for (int j = 0; j < 3; ++j) {
      ws.rgb[j] = kWormRgb[w][j];
    }

    Palette pal;
    pal.Clear();
    pal.SetWormColour(w, ws, ColorMode::kClassic);

    Color real_pal[256];
    pal.Activate(real_pal);

    auto const kRamp = LegacyWormRamp(kWormRgb[w]);

    // 5-entry sprite ramp at base-2 .. base+2.
    for (int j = 0; j < 5; ++j) {
      RequireEntry(real_pal[kBase[w] - 2 + j], kRamp[j]);
    }

    // Secondary 6-entry copy cycles through ramp entries 1..3.
    for (int j = 0; j < 6; ++j) {
      RequireEntry(real_pal[kColourIndex[w] + j], kRamp[1 + j % 3]);
    }

    // Minimap / status colours copy ramp entries 2..4.
    for (int j = 0; j < 3; ++j) {
      RequireEntry(real_pal[129 + w * 4 + j], kRamp[2 + j]);
    }
  }
}

TEST_CASE("classic worm shading quantizes 8-bit colours to the VGA grid", "[palette]") {
  // Colours that only differ below the VGA grid must shade identically.
  WormSettings a;
  WormSettings b;
  int const kColorA[3] = {104, 104, 252};
  int const kColorB[3] = {107, 105, 255};
  for (int j = 0; j < 3; ++j) {
    a.rgb[j] = kColorA[j];
    b.rgb[j] = kColorB[j];
  }

  Palette pal_a;
  Palette pal_b;
  pal_a.Clear();
  pal_b.Clear();
  pal_a.SetWormColour(0, a, ColorMode::kClassic);
  pal_b.SetWormColour(0, b, ColorMode::kClassic);

  Color real_a[256];
  Color real_b[256];
  pal_a.Activate(real_a);
  pal_b.Activate(real_b);

  for (int i = 0; i < 256; ++i) {
    REQUIRE(real_a[i].r == real_b[i].r);
    REQUIRE(real_a[i].g == real_b[i].g);
    REQUIRE(real_a[i].b == real_b[i].b);
  }
}

TEST_CASE("fade endpoints are exact", "[palette]") {
  std::vector<uint8_t> raw(256 * 3);
  for (std::size_t i = 0; i < raw.size(); ++i) {
    raw[i] = static_cast<uint8_t>(i & 63);
  }
  io::MemReader r(raw.data(), raw.size());
  Palette pal;
  pal.Read(r);

  Palette faded = pal;
  faded.Fade(32);  // full brightness: must be a no-op
  Color a[256];
  Color b[256];
  pal.Activate(a);
  faded.Activate(b);
  for (int i = 0; i < 256; ++i) {
    REQUIRE(a[i].r == b[i].r);
    REQUIRE(a[i].g == b[i].g);
    REQUIRE(a[i].b == b[i].b);
  }

  faded.Fade(0);  // fully faded: everything black
  faded.Activate(b);
  for (auto const& e : b) {
    REQUIRE(static_cast<int>(e.r) == 0);
    REQUIRE(static_cast<int>(e.g) == 0);
    REQUIRE(static_cast<int>(e.b) == 0);
  }
}

TEST_CASE("lightup at zero amount is an identity", "[palette]") {
  std::vector<uint8_t> raw(256 * 3);
  for (std::size_t i = 0; i < raw.size(); ++i) {
    raw[i] = static_cast<uint8_t>((i * 5) & 63);
  }
  io::MemReader r(raw.data(), raw.size());
  Palette pal;
  pal.Read(r);

  Palette lit = pal;
  lit.LightUp(0);
  Color a[256];
  Color b[256];
  pal.Activate(a);
  lit.Activate(b);
  for (int i = 0; i < 256; ++i) {
    REQUIRE(a[i].r == b[i].r);
    REQUIRE(a[i].g == b[i].g);
    REQUIRE(a[i].b == b[i].b);
  }
}

TEST_CASE("rotatefrom permutes entries without touching channel values", "[palette]") {
  std::vector<uint8_t> raw(256 * 3);
  for (std::size_t i = 0; i < raw.size(); ++i) {
    raw[i] = static_cast<uint8_t>((i * 11 + 1) & 63);
  }
  io::MemReader r(raw.data(), raw.size());
  Palette source;
  source.Read(r);

  Palette rotated = source;
  rotated.RotateFrom(source, 168, 174, 2);

  for (int i = 168; i <= 174; ++i) {
    int const kCount = 174 - 168 + 1;
    int const kSrc = 168 + ((i - 168) + kCount - 2) % kCount;
    REQUIRE(rotated.entries[i].r == source.entries[kSrc].r);
    REQUIRE(rotated.entries[i].g == source.entries[kSrc].g);
    REQUIRE(rotated.entries[i].b == source.entries[kSrc].b);
  }
}

TEST_CASE("readfull keeps 8-bit channels unclamped", "[palette]") {
  std::vector<uint8_t> raw(256 * 3);
  for (std::size_t i = 0; i < raw.size(); ++i) {
    raw[i] = static_cast<uint8_t>((i * 7 + 3) & 0xff);
  }

  io::MemReader r(raw.data(), raw.size());
  Palette pal;
  pal.ReadFull(r);

  for (int i = 0; i < 256; ++i) {
    REQUIRE(static_cast<int>(pal.entries[i].r) == static_cast<int>(raw[i * 3]));
    REQUIRE(static_cast<int>(pal.entries[i].g) == static_cast<int>(raw[i * 3 + 1]));
    REQUIRE(static_cast<int>(pal.entries[i].b) == static_cast<int>(raw[i * 3 + 2]));
  }
}

TEST_CASE("modern worm shading uses full 8-bit precision", "[palette]") {
  WormSettings ws;
  ws.rgb[0] = 255;
  ws.rgb[1] = 128;
  ws.rgb[2] = 32;

  Palette pal;
  pal.Clear();
  pal.SetWormColour(0, ws, ColorMode::kModern);

  // Full-precision golden reference: (4*add + c*scale) / 64, clamped.
  struct {
    int scale, add;
  } const kSteps[5] = {{.scale = 38, .add = 0},
                       {.scale = 50, .add = 0},
                       {.scale = 64, .add = 0},
                       {.scale = 47, .add = 1008},
                       {.scale = 28, .add = 2205}};
  for (int j = 0; j < 5; ++j) {
    int const kIdx = 30 + j;
    auto scale_add = [&](int c) {
      int const kV = (4 * kSteps[j].add + c * kSteps[j].scale) / 64;
      return static_cast<uint8_t>(kV < 255 ? kV : 255);
    };
    Rgb const kExpected{
        .r = scale_add(ws.rgb[0]), .g = scale_add(ws.rgb[1]), .b = scale_add(ws.rgb[2])};
    RequireEntry(pal.entries[kIdx], kExpected);
  }

  // And it must actually differ from the classic quantized ramp for this
  // off-grid colour.
  Palette classic;
  classic.Clear();
  classic.SetWormColour(0, ws, ColorMode::kClassic);
  bool any_difference = false;
  for (int j = 30; j <= 34; ++j) {
    any_difference = any_difference || classic.entries[j].r != pal.entries[j].r ||
                     classic.entries[j].g != pal.entries[j].g ||
                     classic.entries[j].b != pal.entries[j].b;
  }
  REQUIRE(any_difference);
}

TEST_CASE("out-of-range worm colours are clamped by the ramp math", "[palette]") {
  // The picker once overshot to 256, and net peers can send arbitrary
  // int32 channels; both must shade exactly like 255.
  WormSettings overshoot;
  WormSettings white;
  for (int j = 0; j < 3; ++j) {
    overshoot.rgb[j] = 256 + j * 1000;
    white.rgb[j] = 255;
  }

  for (auto const kMode : {ColorMode::kClassic, ColorMode::kModern}) {
    Palette pal_overshoot;
    Palette pal_white;
    pal_overshoot.Clear();
    pal_white.Clear();
    pal_overshoot.SetWormColour(0, overshoot, kMode);
    pal_white.SetWormColour(0, white, kMode);

    for (int i = 0; i < 256; ++i) {
      REQUIRE(pal_overshoot.entries[i].r == pal_white.entries[i].r);
      REQUIRE(pal_overshoot.entries[i].g == pal_white.entries[i].g);
      REQUIRE(pal_overshoot.entries[i].b == pal_white.entries[i].b);
    }
  }
}

TEST_CASE("expandtofullrange maps the VGA grid onto the full 8-bit range", "[palette]") {
  Palette pal;
  pal.Clear();
  // Classic worm blue, as stored after the 6-bit load expansion.
  pal.entries[1] = {.r = 104, .g = 104, .b = 248, .unused = 0};
  // Brightest classic white (must reach full range).
  pal.entries[2] = {.r = 252, .g = 252, .b = 252, .unused = 0};

  pal.ExpandToFullRange();

  // Colours stay true to the original: (v << 2) | (v >> 4) per channel.
  REQUIRE(static_cast<int>(pal.entries[1].r) == 105);
  REQUIRE(static_cast<int>(pal.entries[1].g) == 105);
  REQUIRE(static_cast<int>(pal.entries[1].b) == 251);

  // Whites reach true 255; black stays black.
  REQUIRE(static_cast<int>(pal.entries[2].r) == 255);
  REQUIRE(static_cast<int>(pal.entries[2].g) == 255);
  REQUIRE(static_cast<int>(pal.entries[2].b) == 255);
  REQUIRE(static_cast<int>(pal.entries[0].r) == 0);
}

TEST_CASE("received levels derive the custom palette flag", "[palette]") {
  Common common;
  Level level(common);

  Palette stock;
  stock.Clear();
  level.origpal.Clear();

  level.DeriveHasCustomPalette(stock);
  REQUIRE(!level.has_custom_palette);

  level.origpal.entries[200].g = 12;
  level.DeriveHasCustomPalette(stock);
  REQUIRE(level.has_custom_palette);
}
