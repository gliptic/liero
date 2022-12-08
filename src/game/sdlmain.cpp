#include <gvl/support/platform.hpp>
#include <exception>
#include <string>

int gameEntry(int argc, char *argv[]);

#if GVL_WINDOWS
#include <windows.h>

INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow) 
{
	try
	{
		return gameEntry(__argc, __argv);
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
