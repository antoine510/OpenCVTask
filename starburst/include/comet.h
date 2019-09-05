#pragma once

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * All functions return < 0 in case of error, 1 in case of success
 * sizes are of type unsigned long which is guaranteed by the standard to be 32bits or more
 */
SATLIB_API void SatLib_Comet_InitModule();
SATLIB_API void SatLib_Comet_CloseModule();

SATLIB_API int SatLib_Comet_Load(const char* cometPath, sbcCometID* cometID);
SATLIB_API int SatLib_Comet_Delete(sbcCometID cometID);

SATLIB_API int SatLib_Comet_ArraySize(sbcEltID eltID, sbcSize* size);
SATLIB_API int SatLib_Comet_ArrayResize(sbcEltID eltID, sbcSize size);

SATLIB_API int SatLib_Comet_Get(sbcEltID eltID, const char* name, sbcEltID* newEltID);

SATLIB_API int SatLib_Comet_SetInteger(sbcEltID eltID, int32_t value);
SATLIB_API int SatLib_Comet_SetBoolean(sbcEltID eltID, int32_t value);
SATLIB_API int SatLib_Comet_SetDecimal(sbcEltID eltID, double value);
SATLIB_API int SatLib_Comet_SetString(sbcEltID eltID, const char* value);
SATLIB_API int SatLib_Comet_SetBuffer(sbcEltID eltID, SatLib_Buffer value);

SATLIB_API int SatLib_Comet_SetArrayInteger(sbcEltID eltID, const int32_t* value, sbcSize size);
SATLIB_API int SatLib_Comet_SetArrayBoolean(sbcEltID eltID, const int32_t* value, sbcSize size);
SATLIB_API int SatLib_Comet_SetArrayDecimal(sbcEltID eltID, const double* value, sbcSize size);
SATLIB_API int SatLib_Comet_SetArrayString(sbcEltID eltID, const char* const* value, sbcSize size);
SATLIB_API int SatLib_Comet_SetArrayBuffer(sbcEltID eltID, const SatLib_Buffer* value, sbcSize size);

SATLIB_API int SatLib_Comet_AsInteger(sbcEltID eltID, int32_t* value);
SATLIB_API int SatLib_Comet_AsBoolean(sbcEltID eltID, int32_t* value);
SATLIB_API int SatLib_Comet_AsDecimal(sbcEltID eltID, double* value);
SATLIB_API int SatLib_Comet_AsString(sbcEltID eltID, const char** value);
SATLIB_API int SatLib_Comet_AsBuffer(sbcEltID eltID, SatLib_Buffer* value);

SATLIB_API int SatLib_Comet_AsArrayElement(sbcEltID eltID, sbcEltID** newEltIDs, sbcSize* newCount);
SATLIB_API int SatLib_Comet_AsArrayInteger(sbcEltID eltID, int32_t** value, sbcSize* size);
SATLIB_API int SatLib_Comet_AsArrayBoolean(sbcEltID eltID, int32_t** value, sbcSize* size);
SATLIB_API int SatLib_Comet_AsArrayDecimal(sbcEltID eltID, double** value, sbcSize* size);
SATLIB_API int SatLib_Comet_AsArrayString(sbcEltID eltID, const char*** value, sbcSize* size);
SATLIB_API int SatLib_Comet_AsArrayBuffer(sbcEltID eltID, SatLib_Buffer** value, sbcSize* size);

#ifdef __cplusplus
}
#endif
