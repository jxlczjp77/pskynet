#ifndef _SKYNET_HARBOR_H_
#define _SKYNET_HARBOR_H_
#include "skynet_config.h"

#define GLOBALNAME_LENGTH 16
#define REMOTE_MAX 256

// 高8位表示节点ID，低24位表示本地ID
#define HANDLE_MASK 0xffffff
#define HANDLE_REMOTE_SHIFT 24

struct remote_name
{
	char name[GLOBALNAME_LENGTH];
	uint32_t handle;
};

struct remote_message
{
	struct remote_name destination;
	const void *message;
	size_t sz;
};

SN_API void skynet_harbor_send(struct remote_message *rmsg, uint32_t source, int session);
SN_API int skynet_harbor_message_isremote(uint32_t handle);
SN_API void skynet_harbor_start(struct skynet_context *ctx);
SN_API void skynet_harbor_exit();

#endif
