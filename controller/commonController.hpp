#ifndef LIERO_CONTROLLER_COMMON_CONTROLLER_HPP
#define LIERO_CONTROLLER_COMMON_CONTROLLER_HPP

#include "controller.hpp"

struct CommonController : Controller
{
	CommonController();
	bool process();
	
	int frameSkip;
	bool inverseFrameSkip;
	int cycles;
};

#endif // LIERO_CONTROLLER_COMMON_CONTROLLER_HPP
