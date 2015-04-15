#ifndef _SKYNET_SOCKET_H_
#define _SKYNET_SOCKET_H_
#include "skynet_config.h"

struct skynet_context;

#define SKYNET_SOCKET_TYPE_DATA 1
#define SKYNET_SOCKET_TYPE_CONNECT 2
#define SKYNET_SOCKET_TYPE_CLOSE 3
#define SKYNET_SOCKET_TYPE_ACCEPT 4
#define SKYNET_SOCKET_TYPE_ERROR 5
#define SKYNET_SOCKET_TYPE_UDP 6

struct skynet_socket_message {
	int type;
	int id;
	int ud;
	char * buffer;
};

void skynet_socket_init();
void skynet_socket_exit();
void skynet_socket_free();
int skynet_socket_poll();

SN_API int skynet_socket_send(struct skynet_context *ctx, int id, void *buffer, int sz);
SN_API void skynet_socket_send_lowpriority(struct skynet_context *ctx, int id, void *buffer, int sz);
SN_API int skynet_socket_listen(struct skynet_context *ctx, const char *host, int port, int backlog);
SN_API int skynet_socket_connect(struct skynet_context *ctx, const char *host, int port);
SN_API int skynet_socket_bind(struct skynet_context *ctx, int fd);
SN_API void skynet_socket_close(struct skynet_context *ctx, int id);
SN_API void skynet_socket_start(struct skynet_context *ctx, int id);
SN_API void skynet_socket_nodelay(struct skynet_context *ctx, int id);

SN_API int skynet_socket_udp(struct skynet_context *ctx, const char * addr, int port);
SN_API int skynet_socket_udp_connect(struct skynet_context *ctx, int id, const char * addr, int port);
SN_API int skynet_socket_udp_send(struct skynet_context *ctx, int id, const char * address, const void *buffer, int sz);
SN_API const char * skynet_socket_udp_address(struct skynet_socket_message *, int *addrsz);

#endif
