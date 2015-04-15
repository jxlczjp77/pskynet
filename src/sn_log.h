#ifndef _SNLOG_H_
#define _SNLOG_H_
#include <stdio.h>
#include "sn_common.h"

#define LOGLEVEL_RAW 0
#define LOGLEVEL_DEBUG 1
#define LOGLEVEL_INFO 2
#define LOGLEVEL_WARNING 3
#define LOGLEVEL_ERROR 4

typedef void(*Logger)(int level, const char *msg);

void LogRaw(const char *fmt, ...);
void LogDebug(const char *fmt, ...);
void LogInfo(const char *fmt, ...);
void LogWarn(const char *fmt, ...);
void LogError(const char *fmt, ...);

FILE *sn_log_open(SNContext *ctx, uint32_t handle);
void sn_log_close(SNContext *ctx, FILE *f, uint32_t handle);
void sn_log_output(FILE *f, uint32_t source, int type, int session, void * buffer, size_t sz);

#endif
