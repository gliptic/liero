#pragma once

#include "exactObjectList.hpp"
#include "math.hpp"

struct Game;

struct Bonus : ExactObjectListBase {
  Bonus() = default;

  fixed x{0};
  fixed y{0};
  fixed vel_y{0};
  int frame{-1};
  int timer{0};
  int weapon{0};

  void Process(Game& game);
};
