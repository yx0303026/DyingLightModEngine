#pragma once

#include <stdint.h>

#if defined(_WIN32)
#define DLMOD_CALL __cdecl
#if defined(DLMOD_ENGINE_BUILD)
#define DLMOD_ENGINE_EXPORT __declspec(dllexport)
#else
#define DLMOD_ENGINE_EXPORT
#endif
#else
#define DLMOD_CALL
#define DLMOD_ENGINE_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define DLMOD_ENGINE_ABI_VERSION 1u

typedef enum DlModResult {
    DLMOD_RESULT_SUCCESS = 0,
    DLMOD_RESULT_INVALID_ARGUMENT = 1,
    DLMOD_RESULT_UNSUPPORTED_ABI = 2,
    DLMOD_RESULT_INVALID_STATE = 3
} DlModResult;

typedef void(DLMOD_CALL* DlModLogFn)(void* context, uint32_t level, const char* message);

typedef struct DlModHostApiV1 {
    uint32_t structSize;
    uint32_t abiVersion;
    void* context;
    DlModLogFn log;
} DlModHostApiV1;

typedef DlModResult(DLMOD_CALL* DlModOnLoadFn)(void* modContext, const DlModHostApiV1* hostApi);
typedef void(DLMOD_CALL* DlModOnUnloadFn)(void* modContext);

/* String fields are copied during registration; context remains owned by the mod until unload. */
typedef struct DlModDescriptorV1 {
    uint32_t structSize;
    uint32_t abiVersion;
    const char* id;
    const char* name;
    const char* version;
    void* context;
    DlModOnLoadFn onLoad;
    DlModOnUnloadFn onUnload;
} DlModDescriptorV1;

typedef DlModResult(DLMOD_CALL* DlModEngineInitializeFn)(const DlModHostApiV1* hostApi);
typedef DlModResult(DLMOD_CALL* DlModEngineRegisterModFn)(const DlModDescriptorV1* mod);
typedef DlModResult(DLMOD_CALL* DlModEngineUnregisterModFn)(const char* modId);
typedef DlModResult(DLMOD_CALL* DlModEngineShutdownFn)(void);

typedef struct DlModEngineApiV1 {
    uint32_t structSize;
    uint32_t abiVersion;
    DlModEngineInitializeFn initialize;
    DlModEngineRegisterModFn registerMod;
    DlModEngineUnregisterModFn unregisterMod;
    DlModEngineShutdownFn shutdown;
} DlModEngineApiV1;

typedef DlModResult(DLMOD_CALL* DlModEngineQueryApiFn)(uint32_t requestedAbiVersion, DlModEngineApiV1* api);

DLMOD_ENGINE_EXPORT DlModResult DLMOD_CALL DlModEngine_QueryApi(
    uint32_t requestedAbiVersion,
    DlModEngineApiV1* api);

#ifdef __cplusplus
}
#endif
