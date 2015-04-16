#ifndef SKYNET_MALLOC_HOOK_H
#define SKYNET_MALLOC_HOOK_H
#include "skynet_config.h"
#include <stdlib.h>

SN_API size_t malloc_used_memory(void);
SN_API size_t malloc_memory_block(void);
SN_API void   memory_info_dump(void);
SN_API size_t mallctl_int64(const char* name, size_t* newval);
SN_API int    mallctl_opt(const char* name, int* newval);
SN_API void   dump_c_mem(void);

#endif /* SKYNET_MALLOC_HOOK_H */

