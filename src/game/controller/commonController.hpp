#pragma once

#include "controller.hpp"

struct CommonController : Controller {
  CommonController();
  bool Process();

  int frame_skip;
  bool inverse_frame_skip;
  int cycles;
};
