#pragma once

#include <ctime>
#include "game.hpp"
#include "math/rect.hpp"
#include "rand.hpp"
#include "viewport.hpp"
#include "worm.hpp"

struct Renderer;

struct SpectatorViewport : Viewport {
  SpectatorViewport(Rect rect, int levwidth, int levheight)
      : Viewport(rect, 0, levwidth, levheight) {}

  void Draw(Game& game, Renderer& renderer, GameState state, bool is_replay) override;
  void Process(Game& game) override;
};
