#include "skynet.h"
#include "sn_server.h"
#include "sn_log.h"
#include "sn_malloc.h"

#define LOG_MESSAGE_SIZE 2048

void skynet_error(skynet_context *context, const char *msg, ...)
{
	static uint32_t logger = 0;
	if (logger == 0) {
		logger = SNServer::Get()->GetHandle().FindName("logger");
	}
	if (logger == 0) {
		return;
	}

	char tmp[LOG_MESSAGE_SIZE];
	char *data = NULL;

	va_list va;
	va_start(va, msg);
	int len = sn_vsprintf(tmp, LOG_MESSAGE_SIZE, msg, va);
	va_end(va);
	if (len != -1 && len < LOG_MESSAGE_SIZE) {
		data = skynet_strdup(tmp);
	}
	else {
		int max_size = LOG_MESSAGE_SIZE;
		for (;;) {
			max_size *= 2;
			data = (char *)sn_malloc(max_size);
			va_start(va, msg);
			len = sn_vsprintf(data, max_size, msg, va);
			va_end(va);
			if (len != -1 && len < max_size) {
				break;
			}
			sn_free(data);
		}
	}

	sn_message smsg;
	if (context == NULL) {
		smsg.source = 0;
	}
	else {
		smsg.source = context->Handle();
	}
	smsg.session = 0;
	smsg.data = data;
	smsg.sz = len | (PTYPE_TEXT << HANDLE_REMOTE_SHIFT);
	SNServer::Get()->ContextPush(logger, &smsg);
}
