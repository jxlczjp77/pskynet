#ifndef _SKYNET_MALLOC_H_
#define _SKYNET_MALLOC_H_
#include "skynet_config.h"

SN_API void *sn_malloc(size_t sz);
SN_API void sn_free(void *ptr);
SN_API void *sn_calloc(size_t nmemb, size_t size);
SN_API void *sn_realloc(void *ptr, size_t size);
SN_API char *skynet_strdup(const char *str);
SN_API void *skynet_lalloc(void *ud, void *ptr, size_t osize, size_t nsize);	// use for lua

#define skynet_malloc sn_malloc
#define skynet_free sn_free

#endif
