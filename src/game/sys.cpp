#include "sys.hpp"
	
#if GVL_WIN32

#include <windows.h>

namespace Win32
{

int getFreeMemory()
{
	MEMORYSTATUS status;
	GlobalMemoryStatus(&status);
	return int(status.dwAvailPhys / 1024);
}

}

#endif
