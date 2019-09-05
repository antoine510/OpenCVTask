#pragma once

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum IGC_PlanetType : int32_t {
	IGC_PLANETTYPE_UNKNOWN = 0,
	IGC_PLANETTYPE_COMPUTER = 1,
	IGC_PLANETTYPE_TABLET = 2,
	IGC_PLANETTYPE_MOBILE = 3
} IGC_PlanetType;

typedef enum IGC_SessionStatus : int32_t {
	IGC_SESSIONSTATUS_STOPPED = 0,
	IGC_SESSIONSTATUS_LAUNCHING = 1,
	IGC_SESSIONSTATUS_RUNNING = 2,
	IGC_SESSIONSTATUS_STOPPING = 3
} IGC_SessionStatus;

typedef enum IGC_SatelliteExecType : int32_t {
	IGC_SATELLITEEXECTYPE_CMD = 0,
	IGC_SATELLITEEXECTYPE_MACOS_APP = 1,
	IGC_SATELLITEEXECTYPE_ANDROID_APP = 2,
	IGC_SATELLITEEXECTYPE_STEAM_APP = 3
} IGC_SatelliteExecType;

typedef struct IGC_Resource {
	char* id;
	void* data;
	uint64_t size;
} IGC_Resource;

typedef struct IGC_Planet {
	char* id;
	char* hostname;
	IGC_PlanetType type;
} IGC_Planet;

typedef struct IGC_EngineClient {
	char* id;
	char* address;
	char* hostname;
} IGC_EngineClient;

typedef struct IGC_Satellite {
	char* id;
	char* name;
	char* title;
	char* iconID;
	char* installedOnID;
	char* engineClientID;
	char** componentIDs;
	uint32_t componentCount;
} IGC_Satellite;

typedef enum IGC_Component_ParamType : int32_t {
	IGC_COMPONENT_PARAMTYPE_INT = 1,
	IGC_COMPONENT_PARAMTYPE_DECIMAL = 2,
	IGC_COMPONENT_PARAMTYPE_STRING = 3,
} IGC_Component_ParamType;

typedef struct IGC_Component_ParamSpec_Int {
	int64_t min;
	int64_t max;
} IGC_Component_ParamSpec_Int;

typedef struct IGC_Component_ParamSpec_Decimal {
	double min;
	double max;
} IGC_Component_ParamSpec_Decimal;

typedef struct IGC_Component_ParamSpec_String {
	char** allowed;
	uint32_t allowedCount;
} IGC_Component_ParamSpec_String;

typedef struct IGC_Component_ParamSpec {
	union {
		IGC_Component_ParamSpec_Int specInt;
		IGC_Component_ParamSpec_Decimal specDecimal;
		IGC_Component_ParamSpec_String specString;
	} specUnion;

	uint32_t specMask;
	char* description;
	IGC_Component_ParamType type;
} IGC_Component_ParamSpec;

typedef struct IGC_Component_ParamValue {
	union {
		int64_t int_;
		double decimal;
		char* string;
	} valueUnion;
	IGC_Component_ParamType type;
} IGC_Component_ParamValue;

typedef struct IGC_Component {
	char* id;
	char* name;
	char* version;
	char* title;
	char* description;
	char* iconID;
	char* bgImageID;
	char** paramNames;
	IGC_Component_ParamSpec* paramSpecs;
	uint32_t paramCount;
} IGC_Component;

typedef struct IGC_Session {
	char* id;
	char* templateID;
	IGC_SessionStatus status;
	char* title;
	char* engineAddress;
} IGC_Session;

typedef struct IGC_Pairing {
	char* id;
	char* componentID;
	char* satelliteID;
	char* sessionID;
	uint32_t count;
	char** paramNames;
	IGC_Component_ParamValue* paramValues;
	uint32_t paramCount;
} IGC_Pairing;

typedef struct IGC_ComponentConstraint {
	char* name;
	char* versionConstraint;
	uint32_t minCount, maxCount;
} IGC_ComponentConstraint;

typedef struct IGC_Template {
	char* id;
	char* name;
	char* title;
	char* description;
	char* bgImageID;
	IGC_ComponentConstraint* constraints;
	uint32_t constraintCount;
} IGC_Template;

SATLIB_API int IGC_InitModule();
SATLIB_API void IGC_CloseModule();

SATLIB_API int IGC_Resource_Get(const char* resourceID, IGC_Resource* resource);

SATLIB_API int IGC_Planet_Get(const char* id, IGC_Planet* planet);
SATLIB_API int IGC_Planet_List(IGC_Planet** planets, uint32_t* planetCount);
SATLIB_API int IGC_Planet_GetLocal(IGC_Planet* planet);

SATLIB_API int IGC_Satellite_Get(const char* id, IGC_Satellite* sat);
SATLIB_API int IGC_Satellite_Register(const char* name, const char** componentIDs, uint32_t componentIDCount,
									  IGC_Satellite* satellite);
SATLIB_API int IGC_Satellite_Deploy(const char* satellitePath);
SATLIB_API int IGC_Satellite_List(IGC_Satellite** satellites, uint32_t* satCount);
SATLIB_API int IGC_Satellite_GetLocal(const char* satelliteName, IGC_Satellite* satellite);

SATLIB_API int IGC_EngineClient_Get(const char* id, IGC_EngineClient* ec);

SATLIB_API int IGC_Component_Register(const char* name, const char* version, const char* title, const char* description,
									  const char* iconName, const char* bgImageName, char** paramNames,
									  IGC_Component_ParamSpec* paramSpecs, uint32_t paramCount,
									  IGC_Component* component);
SATLIB_API int IGC_Component_Get(const char* id, IGC_Component* component);
SATLIB_API int IGC_Component_GetByFields(const char* name, const char* version, IGC_Component* component);
SATLIB_API int IGC_Component_List(IGC_Component** components, uint32_t* componentCount);

SATLIB_API int IGC_Template_Register(const char* name, const char* title, const char* description,
									 const char* bgImageName, IGC_ComponentConstraint* constraints,
									 uint32_t constraintCount, IGC_Template* tmpl);
SATLIB_API int IGC_Template_Deploy(const char* path);
SATLIB_API int IGC_Template_Get(const char* id, IGC_Template* tmpl);
SATLIB_API int IGC_Template_GetByFields(const char* name, IGC_Template* tmpl);
SATLIB_API int IGC_Template_ListForSat(const char* satID, IGC_Template** tmpls, uint32_t* tmplCount);

SATLIB_API int IGC_Session_Get(const char* sessionID, const char* address, IGC_Session* session);
SATLIB_API int IGC_Session_FindForSat(const char* satID, IGC_Session* session);
SATLIB_API int IGC_Session_Create(const char* templateID, const char* sessionName, IGC_Session* session);
SATLIB_API int IGC_Session_Delete(const char* sessionID);
SATLIB_API int IGC_Session_ListForSat(const char* satID, IGC_Session** sessions, uint32_t* sessionCount);
SATLIB_API int IGC_Session_Start(const char* sessionID, const char* address);
SATLIB_API int IGC_Session_Stop(const char* sessionID, const char* address);

SATLIB_API int IGC_Pairing_GetByFields(const char* sessionID, const char* componentID, const char* satelliteID,
									   IGC_Pairing* pairing);
SATLIB_API int IGC_Pairing_ListForSession(const char* sessionID, IGC_Pairing** pairings, uint32_t* count);
SATLIB_API int IGC_Pairing_Register(const char* sessionID, const char* componentID, const char* satelliteID,
									const char** paramNames, IGC_Component_ParamValue* paramValues, uint32_t paramCount,
									IGC_Pairing* pairing);
SATLIB_API int IGC_Pairing_Unregister(const char* pairingID);

#ifdef __cplusplus
}
#endif
