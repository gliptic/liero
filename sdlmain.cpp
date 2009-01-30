#include "platform.hpp"

int gameEntry(int argc, char *argv[]);

#if defined(LIERO_WIN32) && !defined(DISABLE_MAINHACK)
// A bit of a hack to avoid DLL dependencies
#define _WIN32_WINDOWS 0x0410
#define WINVER 0x0410
#include <windows.h>
#include <SDL/SDL.h>

#ifdef main
#undef main
#endif

int main(int argc, char *argv[])
{
	SDL_SetModuleHandle(GetModuleHandle(NULL));
	
	return gameEntry(argc, argv);
}
#else
int main(int argc, char *argv[])
{
	return gameEntry(argc, argv);
}
#endif
