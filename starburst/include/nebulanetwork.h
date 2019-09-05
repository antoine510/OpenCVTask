#pragma once

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum SatLib_Topic_Storage : unsigned int {
	SATLIB_TOPIC_QUEUE = 0,
	SATLIB_TOPIC_STACK = 1,
	SATLIB_TOPIC_STREAM = 2,
} SatLib_Topic_Storage;

typedef void(CALLBACK_CONV* CCometReceivedSignal)(void* data);

SATLIB_API int SatLib_Nebula_InitModule();
SATLIB_API void SatLib_Nebula_CloseModule();

SATLIB_API int SatLib_Nebula_SendComet(const char* topicName, sbcCometID cometID);

SATLIB_API int SatLib_Topic_Subscribe(const char* topicName, SatLib_Topic_Storage storageType, int enableCRS,
									  CCometReceivedSignal ccrs, void* signalData, sbcTopicID* tid);
SATLIB_API int SatLib_Topic_Unsubscribe(sbcTopicID tid);
SATLIB_API int SatLib_Topic_GetComet(sbcTopicID tid, sbcCometID* cometID);
SATLIB_API int SatLib_Topic_WaitComet(sbcTopicID tid, float timeout, sbcCometID* cometID);
SATLIB_API int SatLib_Topic_CometCount(sbcTopicID tid, uint32_t* cometCount);
SATLIB_API int SatLib_Topic_Flush(sbcTopicID tid);

#ifdef __cplusplus
}
#endif
