#pragma once

#include <ctime>
#include "game.hpp"
#include "math/rect.hpp"
#include "rand.hpp"
#include "worm.hpp"

struct Renderer;

struct Viewport {
  Viewport(Rect rect, int worm_idx, int levwidth, int levheight)
      : max_x(levwidth - rect.Width()),
        max_y(levheight - rect.Height()),
        center_x(rect.Width() >> 1),
        center_y(rect.Height() >> 1),
        x(0),
        y(0),
        shake(0),
        worm_idx(worm_idx),
        banner_y(-8),
        rect(rect) {}

  Viewport() = default;

  int x, y;
  int shake;
  int max_x, max_y;
  int center_x, center_y;
  int worm_idx;
  int banner_y;
  Rect rect;
  Rand rand;

  void SetCenter(int x, int y) {
    this->x = x - center_x;
    this->y = y - center_y;
  }

  void ScrollTo(int dest_x, int dest_y, int iter) {
    for (int c = 0; c < iter; c++) {
      if (x < dest_x - center_x)
        ++x;
      else if (x > dest_x - center_x)
        --x;

      if (y < dest_y - center_y)
        ++y;
      else if (y > dest_y - center_y)
        --y;
    }
  }

  virtual void Draw(Game& game, Renderer& renderer, GameState state, bool is_replay);
  virtual void Process(Game& game);
};
