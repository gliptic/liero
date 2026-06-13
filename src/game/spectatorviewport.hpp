#pragma once

#include "game.hpp"
#include "gfx/bitmap.hpp"
#include "math/rect.hpp"
#include "viewport.hpp"
#include "worm.hpp"

struct Renderer;

struct SpectatorViewport : Viewport {
  explicit SpectatorViewport(Rect rect) : Viewport(rect, 0) {}

  void Draw(Game& game, Renderer& renderer, GameState state, bool is_replay) override;
  void Process(Game& game) override;

  // Reused scratch buffer for the world pass; sized to the visible world
  // region each frame and downscaled into the spectator rect.
  Bitmap scratch_bmp;
  // Current zoom factor (1.0 = native, <1.0 = zoomed out). Display-only —
  // never touches the simulation and may use floats freely.
  float zoom{1.0F};
  // Render dimensions cached from the renderer at the start of each Draw()
  // call. Process() reads these so it stays consistent with the current
  // renderer resolution without needing a Renderer& parameter.
  int render_w{640};
  int render_h{400};
};
