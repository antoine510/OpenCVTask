#pragma once

#include <stdint.h>

#ifdef _MSC_VER

#define CALLBACK_CONV __stdcall
#ifdef SATLIB_EXPORT
#define SATLIB_API __declspec(dllexport)
#else
#define SATLIB_API __declspec(dllimport)
#endif

#else

#define CALLBACK_CONV
#ifdef SATLIB_EXPORT
#define SATLIB_API __attribute__((visibility("default")))
#else
#define SATLIB_API
#endif

#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum SatLib_LogMask : uint8_t {
	SATLIB_LOGMASK_DEV = 1,
	SATLIB_LOGMASK_INFO = 2,
	SATLIB_LOGMASK_WARN = 4,
	SATLIB_LOGMASK_ERR = 8,
	SATLIB_LOGMASK_ALL = SATLIB_LOGMASK_DEV | SATLIB_LOGMASK_INFO | SATLIB_LOGMASK_WARN | SATLIB_LOGMASK_ERR
} SatLib_LogMask;

/**
 * C struct image of Buffer !must be same size and order!
 */
typedef struct SatLib_Buffer {
	const char* data;
	unsigned long size;
} SatLib_Buffer;

typedef int32_t sbcTopicID;
typedef uint32_t sbcCometID;
typedef uint64_t sbcEltID;
typedef uint32_t sbcSize;

typedef void(CALLBACK_CONV* CLogCallback)(const char* msg);
typedef const char*(CALLBACK_CONV* CDefinitionLoadCallback)(const char* name);
typedef void(CALLBACK_CONV* CResourceLoadCallback)(const char* name, void** data, uint64_t* size);

SATLIB_API void SatLib_Logger_ClearCallbacks();
SATLIB_API int SatLib_Logger_RegisterCallback(CLogCallback ccb, SatLib_LogMask mask);

SATLIB_API void SatLib_SetDefinitionLoader(CDefinitionLoadCallback loader);
SATLIB_API void SatLib_SetResourceLoader(CResourceLoadCallback loader);

SATLIB_API void SatLib_Logger_LogError(const char* message);

SATLIB_API void SatLib_Free(void* block);

#ifdef __cplusplus
}
#endif
