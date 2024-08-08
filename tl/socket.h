#ifndef UUID_BEDBCF086FF249445BE6E39168A62865
#define UUID_BEDBCF086FF249445BE6E39168A62865

#include <stddef.h>
#include "config.h"
#include "cstdint.h"
#include "platform.h"

#define tl_failure      (-1)
#define tl_would_block  (-2)
#define tl_conn_reset   (-3)
#define tl_disconnected (-4)

typedef union { int _int; void* _voidp; } tl_socket;

typedef struct tl_internet_addr tl_internet_addr;

TL_SOCK_API tl_socket tl_socket_invalid();

TL_SOCK_API int  tl_socket_is_valid(tl_socket sock);
TL_SOCK_API void tl_socket_close();

TL_SOCK_API tl_socket tl_tcp_socket();
TL_SOCK_API tl_socket tl_udp_socket();

TL_SOCK_API void      tl_set_nonblocking(tl_socket sock, int no_blocking);
TL_SOCK_API int       tl_set_nodelay(tl_socket sock, int no_delay);
TL_SOCK_API int       tl_bind(tl_socket sock, int port);
TL_SOCK_API int       tl_listen(tl_socket sock);
TL_SOCK_API tl_socket tl_accept(tl_socket sock, tl_internet_addr* addr);
TL_SOCK_API int       tl_connect(tl_socket sock, tl_internet_addr* addr);

/* Communication */
TL_SOCK_API int tl_send(tl_socket sock, void const* msg, size_t len);
TL_SOCK_API int tl_recv(tl_socket sock, void* msg, size_t len);
TL_SOCK_API int tl_sendto(tl_socket sock, void const* msg, size_t len, tl_internet_addr const* dest);
TL_SOCK_API int tl_recvfrom(tl_socket sock, void* msg, size_t len, tl_internet_addr* src);

TL_SOCK_API int tl_opt_error(tl_socket sock);

#define tl_socket_bind_any(sock) (tl_socket_bind((sock), 0))

#define TL_SS_MAXSIZE (128)                 // Maximum size
#define TL_SS_ALIGNSIZE (sizeof(int64_t)) // Desired alignment
#define TL_SS_PAD1SIZE (TL_SS_ALIGNSIZE - sizeof(short))
#define TL_SS_PAD2SIZE (TL_SS_MAXSIZE - (sizeof(short) + TL_SS_PAD1SIZE + TL_SS_ALIGNSIZE))

typedef struct tl_sockaddr_storage
{
    short ss_family;               // Address family.

    char _ss_pad1[TL_SS_PAD1SIZE];  // 6 byte pad, this is to make
                                   //   implementation specific pad up to
                                   //   alignment field that follows explicit
                                   //   in the data structure
    int64_t _ss_align;            // Field to force desired structure
    char _ss_pad2[TL_SS_PAD2SIZE];  // 112 byte pad to achieve desired size;
                                   //   _SS_MAXSIZE value minus size of
                                   //   ss_family, __ss_pad1, and
                                   //   __ss_align fields is 112
} tl_sockaddr_storage;

typedef struct tl_internet_addr
{
	tl_sockaddr_storage storage_;
} tl_internet_addr;

TL_SOCK_API void tl_internet_addr_init_empty(tl_internet_addr* self); // any
TL_SOCK_API void tl_internet_addr_init_name(tl_internet_addr* self, char const* name, int port);

// NOTE: This may not get the address unless I/O has occured
// on the socket or (if applicable) a connect or accept has
// been done.
TL_SOCK_API void tl_internet_addr_init_from_socket(tl_internet_addr* self, tl_socket s);
TL_SOCK_API void tl_internet_addr_init(tl_internet_addr* self, uint32_t ip, int port);

TL_SOCK_API int tl_internet_addr_valid(tl_internet_addr* self);

TL_SOCK_API int  tl_internet_addr_port(tl_internet_addr const* self);
TL_SOCK_API void tl_internet_addr_set_port(tl_internet_addr* self, int port_new);

TL_SOCK_API uint32_t tl_internet_addr_ip(tl_internet_addr const* self);
TL_SOCK_API void     tl_internet_addr_set_ip(tl_internet_addr* self, uint32_t ip_new);

TL_SOCK_API void tl_internet_addr_reset(tl_internet_addr* self);

TL_SOCK_API int tl_internet_addr_eq(tl_internet_addr const*, tl_internet_addr const*);

#endif // UUID_BEDBCF086FF249445BE6E39168A62865
