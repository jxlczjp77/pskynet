#include "sn_malloc.h"
#include "sn_common.h"

void *sn_malloc(size_t sz)
{
	return malloc(sz);
}

void sn_free(void *ptr)
{
	free(ptr);
}

void *sn_calloc(size_t nmemb, size_t size)
{
	return calloc(nmemb, size);
}

void *sn_realloc(void *ptr, size_t size)
{
	return realloc(ptr, size);
}

char *skynet_strdup(const char *str)
{
	size_t sz = strlen(str);
	char *ret = (char *)sn_malloc(sz + 1);
	memcpy(ret, str, sz + 1);
	ret[sz] = '\0';
	return ret;
}

void *skynet_lalloc(void *ud, void *ptr, size_t osize, size_t nsize)
{
	if (nsize == 0) {
		sn_free(ptr);
		return NULL;
	}
	else {
		return sn_realloc(ptr, nsize);
	}
}
