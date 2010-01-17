
#include "commonController.hpp"

#include "../gfx.hpp"

CommonController::CommonController() 
: frameSkip(1)
, inverseFrameSkip(false)
, cycles(0)
{
}

bool CommonController::process()
{
	int newFrameSkip = 0;
	if(gfx.testSDLKeyOnce(SDLK_1))
		newFrameSkip = 1;
	else if(gfx.testSDLKeyOnce(SDLK_2))
		newFrameSkip = 2;
	else if(gfx.testSDLKeyOnce(SDLK_3))
		newFrameSkip = 4;
	else if(gfx.testSDLKeyOnce(SDLK_4))
		newFrameSkip = 8;
	else if(gfx.testSDLKeyOnce(SDLK_5))
		newFrameSkip = 16;
	else if(gfx.testSDLKeyOnce(SDLK_6))
		newFrameSkip = 32;
	else if(gfx.testSDLKeyOnce(SDLK_7))
		newFrameSkip = 64;
	else if(gfx.testSDLKeyOnce(SDLK_8))
		newFrameSkip = 128;
	else if(gfx.testSDLKeyOnce(SDLK_9))
		newFrameSkip = 256;
	else if(gfx.testSDLKeyOnce(SDLK_0))
		newFrameSkip = 512;
		
	if(newFrameSkip)
	{
		inverseFrameSkip = (gfx.testSDLKey(SDLK_RCTRL) || gfx.testSDLKey(SDLK_LCTRL));
		frameSkip = newFrameSkip;
	}
	
	++cycles;
		
	return true;
}
