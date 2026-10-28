#pragma once

#include <stdint.h>

#if defined(_WIN32)
#define MODSDK_CALL __cdecl
#if defined(MODSDK_BUILD)
#define MODSDK_API __declspec(dllexport)
#else
#define MODSDK_API __declspec(dllimport)
#endif
#define MODSDK_MOD_EXPORT __declspec(dllexport)
#else
#define MODSDK_CALL
#define MODSDK_API
#define MODSDK_MOD_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define MODSDK_ABI_VERSION 1u

typedef enum DlModResult {
    DLMOD_RESULT_SUCCESS = 0,
    DLMOD_RESULT_INVALID_ARGUMENT = 1,
    DLMOD_RESULT_UNSUPPORTED_ABI = 2,
    DLMOD_RESULT_INVALID_STATE = 3,
    DLMOD_RESULT_NOT_FOUND = 4,
    DLMOD_RESULT_IO_ERROR = 5,
    DLMOD_RESULT_PARTIAL_SUCCESS = 6
} DlModResult;

typedef void(MODSDK_CALL* DlModLogFn)(void* context, uint32_t level, const char* message);

typedef struct DlModHostApiV1 {
    uint32_t structSize;
    uint32_t abiVersion;
    void* context;
    DlModLogFn log;
} DlModHostApiV1;

typedef DlModResult(MODSDK_CALL* DlModOnLoadFn)(
    void* modContext,
    const DlModHostApiV1* hostApi);
typedef void(MODSDK_CALL* DlModOnUnloadFn)(void* modContext);

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

/*
 * Every loadable mod DLL must export a function named ModSDK_QueryMod.
 * The manager calls it after loading the DLL to obtain the mod descriptor.
 */
typedef DlModResult(MODSDK_CALL* DlModQueryModFn)(
    uint32_t requestedAbiVersion,
    DlModDescriptorV1* descriptor);

typedef DlModResult(MODSDK_CALL* DlModEngineInitializeFn)(
    const DlModHostApiV1* hostApi);
typedef DlModResult(MODSDK_CALL* DlModEngineRegisterModFn)(
    const DlModDescriptorV1* mod);
typedef DlModResult(MODSDK_CALL* DlModEngineUnregisterModFn)(
    const char* modId);
typedef DlModResult(MODSDK_CALL* DlModEngineShutdownFn)(void);

typedef struct DlModEngineApiV1 {
    uint32_t structSize;
    uint32_t abiVersion;
    DlModEngineInitializeFn initialize;
    DlModEngineRegisterModFn registerMod;
    DlModEngineUnregisterModFn unregisterMod;
    DlModEngineShutdownFn shutdown;
} DlModEngineApiV1;

typedef DlModResult(MODSDK_CALL* ModSDKQueryApiFn)(
    uint32_t requestedAbiVersion,
    DlModEngineApiV1* api);

MODSDK_API DlModResult MODSDK_CALL ModSDK_QueryApi(
    uint32_t requestedAbiVersion,
    DlModEngineApiV1* api);

#ifdef __cplusplus
}
#endif