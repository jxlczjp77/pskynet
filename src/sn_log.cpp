#include "sn_log.h"
#include "sn_common.h"
#include "sn_env.h"
#include "skynet.h"
#include "sn_mq.h"
#include "sn_socket.h"
#include "sn_timer.h"

int sn_sprintf(char *dest, size_t destCount, const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	int nCount = sn_vsprintf(dest, destCount, fmt, va);
	va_end(va);
	return nCount;
}

#ifdef WIN32
#define snprintf _vsnprintf
#endif

int sn_vsprintf(char *dest, size_t destCount, const char *fmt, va_list va)
{
	return snprintf(dest, destCount, fmt, va);
}

void sn_strcpy(char *dest, size_t destCount, const char *src)
{
	strcpy_s(dest, destCount, src);
}

int sn_sscanf(const char *src, const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	return vsscanf(src, fmt, va);
}

static const char *LevelStr(int level)
{
	switch (level)
	{
	case LOGLEVEL_ERROR: return "ERROR";
	case LOGLEVEL_DEBUG: return "DEBUG";
	case LOGLEVEL_WARNING: return "WARN";
	case LOGLEVEL_INFO: return "INFO";
	case LOGLEVEL_RAW:
	default: return "";
	}
}

static std::mutex g_logger_mutex;
static void default_logger(int level, const char *msg)
{
	tm tm_;
	time_t t = time(0);
	localtime_s(&tm_, &t);

	char time_buf[32];
	sn_sprintf(time_buf, 32, "%4d%02d%02d %02d:%02d:%02d ",
		tm_.tm_year + 1900, tm_.tm_mon + 1, tm_.tm_mday, tm_.tm_hour, tm_.tm_min, tm_.tm_sec);

	// 这里处理一下换行，保证每行开始都打印时间(除空行外)
	char *p = (char *)msg;
	while (*p) {
		char *pp = strchr(p, '\n');
		if (pp != NULL) {
			*pp = '\0';
		}

		std::unique_lock<std::mutex> lock(g_logger_mutex);
		if (*p) {
			(level == LOGLEVEL_ERROR ? std::cerr : std::cout)
				<< time_buf << LevelStr(level) << " " << p;
		}

		if (pp == NULL) {
			break;
		}

		(level == LOGLEVEL_ERROR ? std::cerr : std::cout) << std::endl;
		*pp = '\n';
		p = pp + 1;
	}
}

Logger g_pLogger = default_logger;

static void LogOutPut(int level, const char *fmt, va_list va)
{
	char tmp[2048];
	int max_size = sizeof(tmp);
	int nCount = sn_vsprintf(tmp, max_size, fmt, va);
	if (nCount != -1 && nCount < max_size) {
		g_pLogger(level, tmp);
	}
	else {
		for (;;) {
			max_size *= 2;
			char *data = (char *)sn_malloc(max_size);
			nCount = sn_vsprintf(data, max_size, fmt, va);
			if (nCount != -1 && nCount < max_size) {
				g_pLogger(level, data);
				sn_free(data);
				break;
			}
			sn_free(data);
		}
	}
}

void LogRaw(const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	LogOutPut(LOGLEVEL_RAW, fmt, va);
	va_end(va);
}

void LogDebug(const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	LogOutPut(LOGLEVEL_DEBUG, fmt, va);
	va_end(va);
}

void LogInfo(const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	LogOutPut(LOGLEVEL_INFO, fmt, va);
	va_end(va);
}

void LogWarn(const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	LogOutPut(LOGLEVEL_WARNING, fmt, va);
	va_end(va);
}

void LogError(const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	LogOutPut(LOGLEVEL_ERROR, fmt, va);
	va_end(va);
}

FILE *sn_log_open(SNContext *ctx, uint32_t handle)
{
	const char *logpath = skynet_getenv("logpath");
	if (logpath == NULL) {
		return NULL;
	}
		
	size_t sz = strlen(logpath);
#if defined(WIN32) || defined(WIN64)
	char tmp[256];
	assert(sz <= sizeof(tmp) - 16);
#else
	char tmp[sz + 16];
#endif
	sn_sprintf(tmp, sz + 16, "%s/%08x.log", logpath, handle);
	FILE *f = fopen(tmp, "ab");
	if (f) {
		uint32_t starttime = skynet_gettime_fixsec();
		uint32_t currenttime = skynet_gettime();
		time_t ti = starttime + currenttime / 100;
		skynet_error(ctx, "Open log file %s", tmp);
		fprintf(f, "open time: %u %s", currenttime, ctime(&ti));
		fflush(f);
	}
	else {
		skynet_error(ctx, "Open log file %s fail", tmp);
	}
	return f;
}

static void log_blob(FILE *f, void *buffer, size_t sz)
{
	size_t i;
	uint8_t *buf = (uint8_t *)buffer;
	for (i = 0; i != sz; i++) {
		fprintf(f, "%02x", buf[i]);
	}
}

static void log_socket(FILE *f, skynet_socket_message *message, size_t sz)
{
	fprintf(f, "[socket] %d %d %d ", message->type, message->id, message->ud);

	if (message->buffer == NULL) {
		const char *buffer = (const char *)(message + 1);
		sz -= sizeof(*message);
		const char *eol = (const char *)memchr(buffer, '\0', sz);
		if (eol) {
			sz = eol - buffer;
		}
		fprintf(f, "[%*s]", (int)sz, (const char *)buffer);
	}
	else {
		sz = message->ud;
		log_blob(f, message->buffer, sz);
	}
	fprintf(f, "\n");
	fflush(f);
}

void sn_log_close(SNContext *ctx, FILE *f, uint32_t handle)
{
	skynet_error(ctx, "Close log file :%08x", handle);
	fprintf(f, "close time: %u\n", skynet_gettime());
	fclose(f);
}

void sn_log_output(FILE *f, uint32_t source, int type, int session, void *buffer, size_t sz)
{
	if (type == PTYPE_SOCKET) {
		log_socket(f, (skynet_socket_message *)buffer, sz);
	}
	else {
		uint32_t ti = skynet_gettime();
		fprintf(f, ":%08x %d %d %u ", source, type, session, ti);
		log_blob(f, buffer, sz);
		fprintf(f, "\n");
		fflush(f);
	}
}
