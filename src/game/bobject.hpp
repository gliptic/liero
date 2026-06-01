#pragma once

#include "fastObjectList.hpp"
#include "math.hpp"

struct Game;

/*
 * Blood Object
 */
struct BObject {
  bool process(Game& game);

  fixedvec pos, vel;
  int color;
};
