#ifndef _SN_IMPL_H_
#define _SN_IMPL_H_
#include "skynet.h"

struct skynet_config {
	int thread;
	int harbor;
	const char *module_path;
	const char *bootstrap;
	const char *logger;
};

#define THREAD_WORKER 0
#define THREAD_MAIN 1
#define THREAD_SOCKET 2
#define THREAD_TIMER 3
#define THREAD_MONITOR 4

class SNServer;
void skynet_start(SNServer &snserver, skynet_config *config);

#endif
