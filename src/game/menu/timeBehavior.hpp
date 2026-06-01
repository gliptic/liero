#pragma once

#include "integerBehavior.hpp"

struct Common;
struct Menu;

struct TimeBehavior : IntegerBehavior {
  TimeBehavior(Common& common, int& v, int min, int max, int step = 1, bool frames = false)
      : IntegerBehavior(common, v, min, max, step, false), frames(frames) {
    allowEntry = false;
  }

  void onUpdate(Menu& menu, MenuItem& item);

  bool frames;
};
