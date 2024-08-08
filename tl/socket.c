#include "socket.h"
#include "socket_sys.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct sockaddr_in sockaddr_in;
typedef struct sockaddr sockaddr;

static sockaddr_in* get_sockaddr_in(tl_internet_addr* self)
{
	return (sockaddr_in*)(&self->storage_);
}

static sockaddr_in const* get_sockaddr_in_c(tl_internet_addr const* self)
{
	return (sockaddr_in const*)(&self->storage_);
}

static sockaddr* get_sockaddr(tl_internet_addr* self)
{
	return (sockaddr*)(&self->storage_);
}

static sockaddr const* get_sockaddr_c(tl_internet_addr const* self)
{
	return (sockaddr const*)(&self->storage_);
}

static void clear_initialized(tl_internet_addr* self)
{
	memset(&self->storage_, 0, sizeof(self->storage_));
}

#if TL_WINDOWS

static int error() { return WSAGetLastError(); }
#define sckerr_in_progress WSAEINPROGRESS
#define sckerr_would_block WSAEWOULDBLOCK
#define sckerr_conn_reset  WSAECONNRESET

tl_socket tl_socket_invalid()
{
	tl_socket s;
	s._voidp = (void*)INVALID_SOCKET;
	return s;
}

int tl_socket_is_valid(tl_socket s)
{
	return tl_native_socket(s) != INVALID_SOCKET;
}

void tl_socket_close(tl_socket s)
{
	if(tl_socket_is_valid(s))
	{
		closesocket(tl_native_socket(s));
	}
}

int const error_ret = SOCKET_ERROR;

#else

static int error() { return errno; }
#define sckerr_in_progress EINPROGRESS
#define sckerr_would_block EWOULDBLOCK
#define sckerr_conn_reset  ECONNRESET

tl_socket tl_invalid_socket()
{
	tl_socket s;
	s._int = -1;
	return s;
}

int tl_socket_is_valid(tl_socket s)
{
	return tl_native_socket(s) != -1;
}

void tl_socket_close(tl_socket s)
{
	if(tl_socket_is_valid(s))
	{
		close(tl_native_socket(s));
	}
}

int const error_ret = -1;

#endif



#if 0
static char* copy_string(char const* p)
{
	size_t len = std::strlen(p) + 1;
	char* s = new char[len];
	std::memcpy(s, p, len);
	return s;
}

static char** copy_list(char** p)
{
	int i;
	int l = 0;
	char** n;
	for(; p[l]; ++l)
		/* nothing */;

	n = new char*[l + 1];
	for(i = 0; i < l; ++i)
	{
		n[i] = copy_string(p[i]);
	}

	n[l] = 0;

	return n;
}

static char** copy_list_l(char** p, size_t len)
{
	int i;
	int l = 0;
	char** n;
	for(; p[l]; ++l)
		/* nothing */;

	n = new char*[l + 1];
	for(i = 0; i < l; ++i)
	{
		n[i] = new char[len];
		std::memcpy(n[i], p[i], len);
	}

	n[l] = 0;

	return n;
}

static void free_list(char** p)
{
	int i;
	for(i = 0; p[i] != NULL; ++i)
		delete [] p[i];
	delete [] p;
}

struct host_entry_storage : host_entry_impl
{
	hostent v;

	~host_entry_storage();

	void* storage()
	{ return &v; }
};

static host_entry_storage* create_host_entry(hostent const* p)
{
	host_entry_storage* self = new host_entry_storage;
	self->v.h_name = copy_string(p->h_name);
	self->v.h_aliases = copy_list(p->h_aliases);
	self->v.h_addrtype = p->h_addrtype;
	self->v.h_length = p->h_length;
	self->v.h_addr_list = copy_list_l(p->h_addr_list, p->h_length);
	return self;
}

host_entry_storage::~host_entry_storage()
{
	delete [] v.h_name;
	free_list(v.h_aliases);
	free_list(v.h_addr_list);
}

host_entry* resolve_host(char const* name)
{
	hostent* p = gethostbyname( name ); // TODO: This is deprecated IIRC

	if(!p)
		return 0; // ERROR

	return create_host_entry(p);
}

host_entry::host_entry(char const* name)
: ptr(resolve_host(name))
{
}
#endif

tl_socket tl_tcp_socket()
{
	tl_socket s;
	tl_init_sockets();
	s = tl_make_socket(socket(PF_INET, SOCK_STREAM, 0));

	if(!tl_socket_is_valid(s))
		return s;

	tl_set_nonblocking(s, 1);
	return s;
}

tl_socket tl_udp_socket()
{
	tl_socket s;
	tl_init_sockets();
	s = tl_make_socket(socket(PF_INET, SOCK_DGRAM, 0));

	if(!tl_socket_is_valid(s))
		return s;

	tl_set_nonblocking(s, 1);
	return s;
}

void tl_set_nonblocking(tl_socket s, int no_blocking)
{
#if TL_WINDOWS
	unsigned long no_blocking_int = no_blocking;
	ioctlsocket(tl_native_socket(s), FIONBIO, &no_blocking_int);
#else
	fcntl(tl_native_socket(s), F_SETFL, no_blocking ? O_NONBLOCK : 0);
#endif
}

int tl_set_nodelay(tl_socket s, int no_delay)
{
	char no_delay_int = (char)no_delay;
#if !defined(BEOS_NET_SERVER)
	return setsockopt(
		tl_native_socket(s),
		IPPROTO_TCP,
		TCP_NODELAY,
		(char*)(&no_delay_int),
		sizeof(no_delay_int)) == 0;
#else
	return 1;
#endif
}

int tl_bind(tl_socket s, int port)
{
	sockaddr_in addr;
	int ret;
	addr.sin_family = AF_INET;
	addr.sin_port = htons((u_short)port);
	addr.sin_addr.s_addr = INADDR_ANY;
	memset(&(addr.sin_zero), '\0', 8);

	ret = bind(tl_native_socket(s), (sockaddr*)(&addr), sizeof(sockaddr_in)); // TODO: Some way to get size from addr
	return (ret != error_ret);
}

int tl_listen(tl_socket s)
{
	int ret = listen(tl_native_socket(s), 5);
	return (ret != error_ret);
}

tl_socket tl_accept(tl_socket s, tl_internet_addr* addr)
{
	socklen_t sin_size = sizeof(sockaddr_in);

	// TODO: Check for errors
	return tl_make_socket(accept(tl_native_socket(s), get_sockaddr(addr), &sin_size));
}

int tl_connect(tl_socket s, tl_internet_addr* addr)
{
	int r = connect(tl_native_socket(s), get_sockaddr(addr), sizeof(sockaddr_in)); // TODO: Some way to get size from addr

	if(r == error_ret)
	{
		int err = error();

		#if TL_WIN32==1
			if(err != sckerr_would_block)
		#else
			if(err != sckerr_in_progress)
		#endif
				return 0; // ERROR
	}

	return 1;
}

static int translate_comm_ret(int ret)
{
	if(ret == 0)
	{
		return tl_disconnected;
	}
	else if(ret == error_ret || ret < 0)
	{
		int err = error();

#if 0
		if(err != sckerr_would_block)
			printf("Sockerr: %d\n", err);
#endif
		switch(err)
		{
		case sckerr_conn_reset: return tl_conn_reset;
		case sckerr_would_block: return tl_would_block;
		default: return tl_failure;
		}
	}

	return ret;
}

int tl_send(tl_socket s, void const* msg, size_t len)
{
	int ret = send(tl_native_socket(s), (char const*)msg, (int)len, 0);

	return translate_comm_ret(ret);
}

int tl_recv(tl_socket s, void* msg, size_t len)
{
	int ret = recv(tl_native_socket(s), (char*)msg, (int)len, 0);

	return translate_comm_ret(ret);
}

int tl_sendto(tl_socket s, void const* msg, size_t len, tl_internet_addr const* dest)
{
	int ret = sendto(
		tl_native_socket(s),
		(char const*)msg,
		(int)len, 0,
		get_sockaddr_c(dest),
		sizeof(sockaddr));

	return translate_comm_ret(ret);
}

int tl_recvfrom(tl_socket s, void* msg, size_t len, tl_internet_addr* src)
{
	socklen_t fromlen = sizeof(sockaddr);
	int ret = recvfrom(
		tl_native_socket(s),
		(char*)msg,
		(int)len, 0,
		get_sockaddr(src),
		&fromlen);

	return translate_comm_ret(ret);
}

int tl_opt_error(tl_socket s)
{
	int status;
	socklen_t len = sizeof(status);
	getsockopt(tl_native_socket(s), SOL_SOCKET, SO_ERROR, (char*)(&status), &len);
	return status;
}

/*
typedef uint64_t sckimpl_sa_align_t;

std::size_t const sckimpl_sa_maxsize = 32; // IPv6 needs 28 bytes
*/

int tl_internet_addr_port(tl_internet_addr const* addr)
{
	sockaddr_in const* s = get_sockaddr_in_c(addr);
	return ntohs(s->sin_port);
}

uint32_t tl_internet_addr_ip(tl_internet_addr const* addr)
{
	sockaddr_in const* s = get_sockaddr_in_c(addr);
#if TL_WIN32 || GVL_WIN64
	return ntohl(s->sin_addr.S_un.S_addr);
#else
	return ntohl(s->sin_addr.s_addr);
#endif
}

void tl_internet_addr_init(tl_internet_addr* self, uint32_t addr, int port)
{
	sockaddr_in* s = get_sockaddr_in(self);
	clear_initialized(self);

	s->sin_family = AF_INET;
	s->sin_port = htons((u_short)port);
	s->sin_addr.s_addr = htonl(addr);
}

void tl_internet_addr_init_name(tl_internet_addr* self, char const* name, int port)
{
	struct hostent* p = gethostbyname(name);
	clear_initialized(self);

	if(p)
	{
		sockaddr_in* s = get_sockaddr_in(self);

		memmove(&s->sin_addr, p->h_addr_list[0], p->h_length);
		s->sin_family = p->h_addrtype;
		s->sin_port = htons((u_short)port);
	}
}

void tl_internet_addr_init_from_socket(tl_internet_addr* self, tl_socket s)
{
	sockaddr_in addr;
	socklen_t t;
	clear_initialized(self);

	t = sizeof(sockaddr_in);

	if(getsockname(tl_native_socket(s), (sockaddr*)&addr, &t) != error_ret)
	{
		*get_sockaddr_in(self) = addr;
	}
}

void tl_internet_addr_init_empty(tl_internet_addr* self)
{
	sockaddr_in* s;
	clear_initialized(self);

	s = get_sockaddr_in(self);

	s->sin_family = AF_INET;
	s->sin_port = htons(0);
	s->sin_addr.s_addr = htonl(INADDR_ANY);
}

void tl_internet_addr_reset(tl_internet_addr* self)
{
	clear_initialized(self);
}

int tl_internet_addr_valid(tl_internet_addr* self)
{
	return get_sockaddr(self)->sa_family != 0;
}

int tl_internet_addr_eq(tl_internet_addr const* a, tl_internet_addr const* b)
{
	return 0 == memcmp(&a->storage_, &b->storage_, sizeof(a->storage_));
}

void tl_internet_addr_set_port(tl_internet_addr* self, int port)
{
	sockaddr_in* s = get_sockaddr_in(self);
	s->sin_port = htons((u_short)port);
}

void tl_internet_addr_set_ip(tl_internet_addr* self, uint32_t addr)
{
	sockaddr_in* s = get_sockaddr_in(self);
	s->sin_addr.s_addr = htonl(addr);
}
