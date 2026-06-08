#pragma once

#include "controller.hpp"

struct CommonController : Controller {
  CommonController();
  bool Process() override;

  int frame_skip{1};
  bool inverse_frame_skip{false};
  int cycles{0};
};
