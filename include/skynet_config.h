#ifndef _SKYNET_CONFIG_H_
#define _SKYNET_CONFIG_H_
#include <stdint.h>

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

#if defined(_MSC_VER) && !defined(__cplusplus)
#define inline __inline
#endif

SN_API int sn_sprintf(char *dest, size_t destCount, const char *fmt, ...);
SN_API int sn_vsprintf(char *dest, size_t destCount, const char *fmt, va_list va);
SN_API void sn_strcpy(char *dest, size_t destCount, const char *src);
SN_API int sn_sscanf(const char *src, const char *fmt, ...);

#ifdef _MSC_VER
SN_API char* sn_strsep(char** stringp, const char* delim);
#define strsep sn_strsep
#endif // _MSC_VER

#endif