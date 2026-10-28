#pragma once

#include "../../EngineAbi.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DLMOD_FILE_SYSTEM_ABI_VERSION 1u

typedef enum DlPakLoaderFlags {
    DLMOD_PAK_LOADER_SCAN_DEFAULT_DIRECTORY = 1u << 0,
    DLMOD_PAK_LOADER_WRITE_DEFAULT_CONFIG = 1u << 1,
    DLMOD_PAK_LOADER_RESOLVE_GAME_DIRECTORY = 1u << 2,
    DLMOD_PAK_LOADER_RESOLVE_MODULE_DIRECTORY = 1u << 3,
    DLMOD_PAK_LOADER_DOT_PREFIX_FALLBACK = 1u << 4,
    DLMOD_PAK_LOADER_ADDITIONAL_SOURCES_FIRST = 1u << 5
} DlPakLoaderFlags;

typedef DlModResult(MODSDK_CALL* DlPakWaitForFileSystemFn)(
    void* context,
    uint32_t timeoutMilliseconds);
typedef DlModResult(MODSDK_CALL* DlPakMountSourceFn)(
    void* context,
    const char* sourceUtf8,
    uint32_t mountFlags,
    uint32_t* lastError);
typedef DlModResult(MODSDK_CALL* DlPakGetAdditionalSourceFn)(
    void* context,
    uint32_t index,
    const char** sourceUtf8);
typedef void(MODSDK_CALL* DlPakLoaderLogFn)(
    void* context,
    uint32_t level,
    const char* messageUtf8);

/*
 * The consumer supplies these operations. The ABI does not resolve game
 * modules, mount sources, create threads, or handle errors on its behalf.
 */
typedef struct DlFileSystemPakProviderV1 {
    uint32_t structSize;
    uint32_t abiVersion;
    void* context;
    DlPakWaitForFileSystemFn waitUntilReady;
    DlPakMountSourceFn mountSource;
    DlPakGetAdditionalSourceFn getAdditionalSource;
    DlPakLoaderLogFn log;
} DlFileSystemPakProviderV1;

/*
 * All strings and source entries are UTF-8 and remain owned by the caller.
 * The caller's loader implementation defines how option flags are applied.
 */
typedef struct DlPakLoaderOptionsV1 {
    uint32_t structSize;
    uint32_t abiVersion;
    uint32_t flags;
    uint32_t mountFlags;
    const char* configurationPathUtf8;
    const char* moduleDirectoryUtf8;
    const char* gameDirectoryUtf8;
    const char* defaultPakDirectoryUtf8;
    const char* enabledKeyUtf8;
    const char* pakPathKeyUtf8;
    const char* defaultSourcePrefixUtf8;
    const char* const* additionalSourcesUtf8;
    uint32_t additionalSourceCount;
    uint32_t fileSystemTimeoutMilliseconds;
    uint32_t mountRetryCount;
    uint32_t mountRetryDelayMilliseconds;
} DlPakLoaderOptionsV1;

typedef struct DlPakLoaderReportV1 {
    uint32_t structSize;
    uint32_t sourcesFound;
    uint32_t sourcesMounted;
    uint32_t sourcesFailed;
    uint32_t lastError;
} DlPakLoaderReportV1;

/*
 * A caller-owned implementation may expose this signature to load configured
 * paks using the provider and options above. It is intentionally not exported
 * by the engine: the module that knows its game and resource layout owns it.
 */
typedef DlModResult(MODSDK_CALL* DlPakLoadConfiguredFn)(
    const DlPakLoaderOptionsV1* options,
    const DlFileSystemPakProviderV1* fileSystem,
    DlPakLoaderReportV1* report);

typedef struct DlPakLoaderApiV1 {
    uint32_t structSize;
    uint32_t abiVersion;
    DlPakLoadConfiguredFn loadConfigured;
} DlPakLoaderApiV1;

#ifdef __cplusplus
}
#endif
