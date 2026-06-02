#include "commonController.hpp"

#include "../gfx.hpp"

CommonController::CommonController() : frame_skip(1), inverse_frame_skip(false), cycles(0) {}

bool CommonController::Process() {
  int new_frame_skip = 0;
  if (gfx.TestSdlKeyOnce(SDL_SCANCODE_1))
    new_frame_skip = 1;
  else if (gfx.TestSdlKeyOnce(SDL_SCANCODE_2))
    new_frame_skip = 2;
  else if (gfx.TestSdlKeyOnce(SDL_SCANCODE_3))
    new_frame_skip = 4;
  else if (gfx.TestSdlKeyOnce(SDL_SCANCODE_4))
    new_frame_skip = 8;
  else if (gfx.TestSdlKeyOnce(SDL_SCANCODE_5))
    new_frame_skip = 16;
  else if (gfx.TestSdlKeyOnce(SDL_SCANCODE_6))
    new_frame_skip = 32;
  else if (gfx.TestSdlKeyOnce(SDL_SCANCODE_7))
    new_frame_skip = 64;
  else if (gfx.TestSdlKeyOnce(SDL_SCANCODE_8))
    new_frame_skip = 128;
  else if (gfx.TestSdlKeyOnce(SDL_SCANCODE_9))
    new_frame_skip = 256;
  else if (gfx.TestSdlKeyOnce(SDL_SCANCODE_0))
    new_frame_skip = 512;

  if (new_frame_skip) {
    inverse_frame_skip = (gfx.TestSdlKey(SDL_SCANCODE_RCTRL) || gfx.TestSdlKey(SDL_SCANCODE_LCTRL));
    frame_skip = new_frame_skip;
  }

  ++cycles;

  return true;
}
