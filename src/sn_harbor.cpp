#include "sn_harbor.h"
#include "sn_handle.h"
#include "sn_server.h"

static SNContextPtr REMOTE;
static unsigned int HARBOR = INVALID_HANDLE;

void skynet_harbor_send(remote_message *rmsg, uint32_t source, int session)
{
	int type = rmsg->sz >> HANDLE_REMOTE_SHIFT;
	rmsg->sz &= HANDLE_MASK;
	assert(type != PTYPE_SYSTEM && type != PTYPE_HARBOR && REMOTE);
	REMOTE->Push(rmsg, sizeof(*rmsg), source, type, session);
}

int skynet_harbor_message_isremote(uint32_t handle)
{
	assert(HARBOR != INVALID_HANDLE);
	int h = (handle & ~HANDLE_MASK);
	return h != HARBOR && h != 0;
}

void skynet_harbor_init(int harbor)
{
	HARBOR = (unsigned int)harbor << HANDLE_REMOTE_SHIFT;
}

void skynet_harbor_start(SNContext *ctx)
{
	REMOTE = ctx->shared_from_this();
}

void skynet_harbor_exit()
{
	REMOTE.reset();
}
