#pragma once

#include <cstdint>

#include "../common.hpp"
#include "../level.hpp"

// Resolves shadow/material queries against the level instead of reading the
// screen as a material surface (the screen stops being palette-indexed in
// the ARGB world). screen + world_offset = level coordinates. The accepted
// semantic change: shadows and explosion masks key off terrain, not off
// whatever sprite happened to be drawn earlier on the screen.
//
// When terrain has an ARGB display layer, ShadowedArgb becomes a darkened
// display_data sample; the call sites stay unchanged.
struct ShadowQuery {
  Common const& common;
  Level const& level;
  uint32_t const* pal32;
  int world_offset_x;
  int world_offset_y;
  ColorMode mode{ColorMode::kClassic};
  int cycles{0};

  // Palette index of the level pixel under screen (sx, sy), or -1 outside
  // the level.
  int PixelAt(int sx, int sy) const {
    int const kWx = sx + world_offset_x;
    int const kWy = sy + world_offset_y;
    if (!level.Inside(kWx, kWy)) {
      return -1;
    }
    return level.material_id[kWx + kWy * level.width];
  }

  // Shadowed palette index for screen (sx, sy) — the level pixel shifted to
  // its darkened palette entry — or -1 if no shadow falls there.
  int ShadowedIndex(int sx, int sy) const {
    int const kP = PixelAt(sx, sy);
    if (kP < 0 || !common.materials[kP].SeeShadow()) {
      return -1;
    }
    // The +4 shadow offset can leave the 256-entry palette for materials near
    // the top of the range; keep the index in bounds.
    return kP + 4 < 256 ? kP + 4 : kP;
  }

  // ARGB to paint at screen (sx, sy) if a shadow falls there, else 0.
  // In modern mode, authored display pixels are darkened in-place (channels
  // halved) instead of shifted to a palette shadow entry.
  uint32_t ShadowedArgb(int sx, int sy) const {
    int const kWx = sx + world_offset_x;
    int const kWy = sy + world_offset_y;
    if (!level.Inside(kWx, kWy)) {
      return 0;
    }
    int const kLevelIdx = kWx + kWy * level.width;
    int const kP = level.material_id[kLevelIdx];
    if (!common.materials[kP].SeeShadow()) {
      return 0;
    }
    if (mode == ColorMode::kModern && !level.display_valid.empty() &&
        level.display_valid[kLevelIdx]) {
      uint32_t const kArgb = level.ResolveDisplayAt(kLevelIdx, cycles);
      return 0xFF000000U | ((kArgb & 0x00FEFEFE) >> 1);
    }
    return pal32[kP + 4 < 256 ? kP + 4 : kP];
  }
};
