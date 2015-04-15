#ifndef _SKYNET_H_
#define _SKYNET_H_
#include "skynet_config.h"
#include "skynet_malloc.h"

#if defined(SN_BUILD_AS_DLL)
#if defined(SN_CORE) || defined(SN_LIB)
#ifdef __cplusplus
#define SN_API extern "C" __declspec(dllexport)
#else
#define SN_API __declspec(dllexport)
#endif
#else
#ifdef __cplusplus
#define SN_API extern "C" __declspec(dllimport)
#else
#define SN_API __declspec(dllimport)
#endif
#endif
#else
#define SN_API		extern
#endif
#define SNLIB_API SN_API

#define PTYPE_TEXT 0
#define PTYPE_RESPONSE 1
#define PTYPE_MULTICAST 2
#define PTYPE_CLIENT 3
#define PTYPE_SYSTEM 4
#define PTYPE_HARBOR 5
#define PTYPE_SOCKET 6
// read lualib/skynet.lua examples/simplemonitor.lua
#define PTYPE_ERROR 7	
// read lualib/skynet.lua lualib/mqueue.lua lualib/snax.lua
#define PTYPE_RESERVED_QUEUE 8
#define PTYPE_RESERVED_DEBUG 9
#define PTYPE_RESERVED_LUA 10
#define PTYPE_RESERVED_SNAX 11

#define PTYPE_TAG_DONTCOPY 0x10000
#define PTYPE_TAG_ALLOCSESSION 0x20000

struct SNContext;
#define skynet_context SNContext
#define skynet_context_handle(ctx) ctx->Handle()
#define skynet_context_push(handle, message) SNServer::Get()->ContextPush(handle, message)
#define skynet_message sn_message

SN_API int skynet_main(int argc, char *argv[]);

SN_API void skynet_error(struct skynet_context *context, const char *msg, ...);
SN_API const char *skynet_command(struct skynet_context *context, const char *cmd, const char *parm);
SN_API uint32_t skynet_queryname(struct skynet_context * context, const char * name);
SN_API int skynet_send(struct skynet_context *context, uint32_t source, uint32_t destination, int type, int session, void * msg, size_t sz);
SN_API int skynet_sendname(struct skynet_context *context, uint32_t source, const char * addr, int type, int session, void * data, size_t sz);

typedef int(*skynet_cb)(struct skynet_context * context, void *ud, int type, int session, uint32_t source, const void * msg, size_t sz);
SN_API void skynet_callback(struct skynet_context * context, void *ud, skynet_cb cb);


SN_API int skynet_isremote(struct skynet_context *, uint32_t handle, int * harbor);
SN_API uint32_t skynet_current_handle(void);

#endif
