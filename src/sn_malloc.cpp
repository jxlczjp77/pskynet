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
