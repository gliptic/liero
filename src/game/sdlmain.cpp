#include <gvl/support/platform.hpp>
#include <exception>
#include <string>

int gameEntry(int argc, char *argv[]);

#if GVL_WINDOWS
#include <windows.h>
#include <SDL.h>

#ifdef main
#undef main
#endif

int main(int argc, char *argv[])
{
	try
	{
		return gameEntry(argc, argv);
	}
	catch(std::exception& ex)
	{
		MessageBoxA(NULL, (std::string("Sorry, something went wrong :(\r\n\r\n") + ex.what()).c_str(), "Liero", MB_OK | MB_ICONWARNING);
		return 1;
	}
}
#else
int main(int argc, char *argv[])
{
	return gameEntry(argc, argv);
}
#endif
