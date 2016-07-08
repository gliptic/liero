#ifndef UUID_338826A0A88B40CBD1DE1AA4A2F6B6CD
#define UUID_338826A0A88B40CBD1DE1AA4A2F6B6CD

#include "socket.h"

#if TL_WINDOWS
#undef  NOGDI
#define NOGDI
#undef  NOMINMAX
#define NOMINMAX
#undef  WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#undef  NONAMELESSUNION
#define NONAMELESSUNION
#undef  NOKERNEL
#define NOKERNEL
#undef  NONLS
#define NONLS

/*
#ifndef POINTER_64
#define POINTER_64 // Needed for bugged headers
#endif*/

#if TL_WIN32
#define _WIN32_WINDOWS 0x0410
#endif

#define WINVER 0x0410
#include <winsock2.h>
#include <stdlib.h>

#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif

#define tl_native_socket(s) ((SOCKET)(s)._voidp)

static int tl_sockets_initialized = 0;

static void tl_init_sockets()
{
	if(!tl_sockets_initialized)
	{
		WSADATA wsaData;
		int res;

		tl_sockets_initialized = 1;

		res = WSAStartup(MAKEWORD(2,2), &wsaData);
		if (res != 0)
			return;
	}
}

TL_INLINE tl_socket tl_make_socket(SOCKET native)
{
	tl_socket s;
	s._voidp = (void*)native;
	return s;
}

typedef int socklen_t;

#else //if !defined(OSK)

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

static void tl_init_sockets()
{
	// Do nothing
}

#define tl_native_socket(s) ((s)._int)

TL_INLINE tl_socket tl_make_socket(int native)
{
	tl_socket s;
	s._int = native;
	return s;
}

#endif

#endif // UUID_338826A0A88B40CBD1DE1AA4A2F6B6CD
