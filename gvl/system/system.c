#include "system.hpp"

#include "../support/platform.hpp"
//#include <stdexcept>
#include "stdio.h"
#include "stdlib.h"

#if GVL_WIN32 || GVL_WIN64

#include <windows.h>
#include <mmsystem.h>

#ifdef _MSC_VER
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "kernel32.lib")
#endif

uint32_t gvl_get_ticks()
{
	static int setup = 0;
	if(!setup)
	{
		TIMECAPS caps;
		setup = 1;
		
		if(timeGetDevCaps(&caps, sizeof(caps)) == TIMERR_NOERROR)
		{
			timeBeginPeriod(min(max(caps.wPeriodMin, 1), caps.wPeriodMax)); 
		}
	}
	
	return (uint32_t)(timeGetTime());
}

uint64_t gvl_get_hires_ticks()
{
	LARGE_INTEGER res;
	QueryPerformanceCounter(&res);
	
	return (uint64_t)res.QuadPart;
}

uint64_t gvl_hires_ticks_per_sec()
{
	LARGE_INTEGER res;
	QueryPerformanceFrequency(&res);
	
	return (uint64_t)res.QuadPart;
}

void gvl_sleep(uint32_t ms)
{
	Sleep((DWORD)ms);
}

#else // !(GVL_WIN32 || GVL_WIN64)

#include <unistd.h>

#if defined(_POSIX_MONOTONIC_CLOCK)

#include <time.h>

uint32_t gvl_get_ticks()
{
	struct timespec t;
	int ret = clock_gettime(CLOCK_MONOTONIC, &t);
	if(ret < 0)
	{
		fprintf(stderr, "clock_gettime failed");
		exit(1);
	}
	return t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

uint64_t gvl_get_hires_ticks()
{
	struct timespec t;
	int ret = clock_gettime(CLOCK_MONOTONIC, &t);
	if(ret < 0)
	{
		fprintf(stderr, "clock_gettime failed");
		exit(1);
	}
	return t.tv_sec * (uint64_t)(1000000000ull) + t.tv_nsec;
}

uint64_t gvl_hires_ticks_per_sec()
{
	return (uint64_t)(1000000000ull);
}
#else // !defined(_POSIX_MONOTONIC_CLOCK)
uint32_t gvl_get_ticks()
{
	//passert(false, "STUB");
	return 0;
}
#endif // !defined(_POSIX_MONOTONIC_CLOCK)

#if GVL_LINUX

#include <time.h>
#include <errno.h>

void gvl_sleep(uint32_t ms)
{
	struct timespec t, left;
	t.tv_sec = ms / 1000;
	t.tv_nsec = (ms % 1000) * 1000000;
	while(nanosleep(&t, &left) == -1
	&& errno == EINTR)
	{
		t = left;
	}
}
#else // !GVL_LINUX
void gvl_sleep(uint32_t)
{
	//passert(false, "STUB");
}
#endif // !GVL_LINUX

#endif // !(GVL_WIN32 || GVL_WIN64)
