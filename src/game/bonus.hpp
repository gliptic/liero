#pragma once

#include "exactObjectList.hpp"
#include "math.hpp"

struct Game;

struct Bonus : ExactObjectListBase {
  Bonus() : frame(-1), x(0), y(0), vel_y(0), timer(0), weapon(0) {}

  fixed x;
  fixed y;
  fixed vel_y;
  int frame;
  int timer;
  int weapon;

  void Process(Game& game);
};
