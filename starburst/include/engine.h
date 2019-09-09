#pragma once

#include "common.h"
#include "igc.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum SatLib_Component_Action : unsigned int {
	SATLIB_COMPONENT_NO_ACTION = 0,
	SATLIB_COMPONENT_STOP = 1,
	SATLIB_COMPONENT_START = 2,
} SatLib_Component_Action;

typedef enum SatLib_Component_State : unsigned int {
	SATLIB_COMPONENT_STOPPED = 0,
	SATLIB_COMPONENT_STARTED = 1
} SatLib_Component_State;

typedef void(CALLBACK_CONV* CActionCallback)(SatLib_Component_Action action, void* aData);

SATLIB_API int SatLib_Engine_InitModule(const char* satelliteName);
SATLIB_API void SatLib_Engine_CloseModule();

SATLIB_API int SatLib_Engine_IsLocalRunning();
SATLIB_API int SatLib_Engine_JoinSession(const char* sessionID, const char* address);
SATLIB_API int SatLib_Engine_LeaveSession();

SATLIB_API int SatLib_Component_Setup(const char* path, CActionCallback acb, void* actionData, IGC_Component_Future* compFuture);
SATLIB_API int SatLib_Component_ReportState(const char* id, SatLib_Component_State newState);
SATLIB_API int SatLib_Component_GetSessionPairing(const char* id, IGC_Pairing* pairing);

#ifdef __cplusplus
}
#endif
