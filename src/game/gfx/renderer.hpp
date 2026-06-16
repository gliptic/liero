#pragma once

#include "../common.hpp"
#include "../rand.hpp"
#include "bitmap.hpp"

struct Renderer {
  Renderer() = default;

  void Init(int x, int y);
  void Clear();
  void LoadPalette(Common const& common);
  void SetRenderResolution(int x, int y);
  // Repacks `pal` into `pal32`. Every palette-rebuild block must end with
  // this, and must run before anything draws into `bmp` for the frame —
  // blits resolve palette indices through `pal32` at draw time.
  void UpdatePal32();

  // The palette `pal` is rebuilt from every frame, picked by `mode`.
  Palette const& Origpal() const { return mode == ColorMode::kModern ? origpal_modern : origpal; }

  // the bitmap that is drawn into by this renderer
  Bitmap bmp;
  Palette pal;
  // ARGB8888 (0xFF000000 | r<<16 | g<<8 | b) image of `pal`, frame scope.
  uint32_t pal32[256] = {};
  // Classic palette origin: the EXE/TC palette, or a level's custom palette.
  Palette origpal;
  // Modern palette origin: the TC's modern.pal (or a full-range expansion of
  // the classic palette when no modern.pal ships).
  Palette origpal_modern;
  // Live colour mode of this renderer; Settings hold the default, each
  // renderer can be switched independently (e.g. spectator vs play screen).
  ColorMode mode{ColorMode::kClassic};
  int fade_value{0};
  // Resolution to render the game at. This should be modified via
  // setRenderResolution() to ensure that the bitmap is re-allocated
  int render_res_x = 320;
  int render_res_y = 200;

  // ── Spectator GPU world composite ────────────────────────────────────────
  // When true, the spectator viewport hands its 1:1 world pass to the GPU for
  // scaling instead of a CPU box-filter into `bmp`, and clears `bmp` to a
  // transparent HUD-only overlay. Set only for the live spectator window; stays
  // false for CPU paths (videotool, single-screen replay, dummy driver), which
  // keep compositing into `bmp`.
  bool gpu_world_composite = false;
  // The 1:1 world pass for the GPU composite, owned by the SpectatorViewport
  // (valid until its next Draw). Null when no world layer was emitted this
  // frame — Gfx then falls back to the CPU present path.
  Bitmap const* gpu_world_src = nullptr;
  // Used sub-rect of `gpu_world_src` actually drawn (≤ the texture size).
  int gpu_world_used_w = 0;
  int gpu_world_used_h = 0;
  // Texture size to allocate (level dims clamped to the ceiling, once per
  // level). See SpectatorWorldTextureSize.
  int gpu_world_max_w = 0;
  int gpu_world_max_h = 0;
  // Centred, letterboxed destination rect for the world blit (window pixels).
  int gpu_world_dst_x = 0;
  int gpu_world_dst_y = 0;
  int gpu_world_dst_w = 0;
  int gpu_world_dst_h = 0;

  // ── Spectator HUD overlay partial present ───────────────────────────────────
  // The HUD overlay only touches a few full-width rows; the spectator viewport
  // clears and the GPU present uploads just these bands instead of the whole
  // window-sized overlay. When hud_overlay_full_refresh is true (first frame /
  // resolution change) the whole overlay is cleared and uploaded so the
  // never-touched regions start transparent. Bands are kept as plain ints to
  // avoid a renderer→spectator header dependency; see ComputeHudDirtyBands.
  bool hud_overlay_full_refresh = true;
  static constexpr int kMaxHudBands = 3;
  int hud_overlay_band_count = 0;
  int hud_overlay_band_y[kMaxHudBands] = {};
  int hud_overlay_band_h[kMaxHudBands] = {};
};
