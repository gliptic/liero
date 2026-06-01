#pragma once

#include "controller.hpp"

struct CommonController : Controller {
  CommonController();
  bool process();

  int frameSkip;
  bool inverseFrameSkip;
  int cycles;
};
