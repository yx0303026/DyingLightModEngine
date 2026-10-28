#pragma once

#include <stdint.h>

#include "EngineAbi.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DLMOD_FILE_SYSTEM_ABI_VERSION 1u
#define DLMOD_RWDI_FILESYSTEM_MODULE_NAME_UTF8 "filesystem_x64_rwdi.dll"
#define DLMOD_RWDI_FILESYSTEM_ADD_SOURCE_SYMBOL \
	"?add_source@fs@@YA_NPEBDW4ENUM@FFSAddSourceFlags@@@Z"
#define DLMOD_RWDI_FILESYSTEM_DEFAULT_PAK_FLAGS 9

	typedef enum DlPakLoaderFlags {
		DLMOD_PAK_LOADER_SCAN_DEFAULT_DIRECTORY = 1u << 0,
		DLMOD_PAK_LOADER_WRITE_DEFAULT_CONFIG = 1u << 1,
		DLMOD_PAK_LOADER_RESOLVE_GAME_DIRECTORY = 1u << 2,
		DLMOD_PAK_LOADER_RESOLVE_MODULE_DIRECTORY = 1u << 3,
		DLMOD_PAK_LOADER_DOT_PREFIX_FALLBACK = 1u << 4,
		DLMOD_PAK_LOADER_ADDITIONAL_SOURCES_FIRST = 1u << 5,
		DLMOD_PAK_LOADER_VALIDATE_RELATIVE_SOURCES = 1u << 6
	} DlPakLoaderFlags;

	/*
	 * This is the game-owned function resolved from the target game's
	 * filesystem_x64_rwdi.dll. Its symbol is build-specific. The caller resolves
	 * it with GetProcAddress and owns the module lifetime; the engine never does.
	 */
#ifdef __cplusplus
	typedef bool(MODSDK_CALL* DlFileSystemAddSourceFn)(
		const char* sourceUtf8,
		int32_t mountFlags);
#else
	typedef uint8_t(MODSDK_CALL* DlFileSystemAddSourceFn)(
		const char* sourceUtf8,
		int32_t mountFlags);
#endif

	typedef struct DlFileSystemRuntimeV1 {
		uint32_t structSize;
		uint32_t abiVersion;
		void* moduleHandle;
		DlFileSystemAddSourceFn addSource;
		int32_t pakMountFlags;
	} DlFileSystemRuntimeV1;

	/*
	 * The caller performs module polling and symbol lookup, then fills runtime.
	 * Success requires a live module handle and a non-null addSource function.
	 */
	typedef DlModResult(MODSDK_CALL* DlPakResolveFileSystemFn)(
		void* context,
		uint32_t timeoutMilliseconds,
		DlFileSystemRuntimeV1* runtime);
	typedef DlModResult(MODSDK_CALL* DlPakMountSourceFn)(
		void* context,
		const char* sourceUtf8,
		int32_t mountFlags,
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
	 * getAdditionalSource is called with increasing indices until it returns
	 * DLMOD_RESULT_NOT_FOUND; the callback may extract an embedded pak first.
	 */
	typedef struct DlFileSystemPakProviderV1 {
		uint32_t structSize;
		uint32_t abiVersion;
		void* context;
		DlPakResolveFileSystemFn resolveFileSystem;
		DlPakMountSourceFn mountSource;
		DlPakGetAdditionalSourceFn getAdditionalSource;
		DlPakLoaderLogFn log;
	} DlFileSystemPakProviderV1;

	/*
	 * All strings and source entries are UTF-8 and remain owned by the caller.
	 * For relative sources, candidate order is game directory, module directory,
	 * original source, then the optional dot-prefix fallback for OnlineMod paths.
	 * If requested, additional sources precede configured and scanned sources.
	 */
	typedef struct DlPakLoaderOptionsV1 {
		uint32_t structSize;
		uint32_t abiVersion;
		uint32_t flags;
		int32_t mountFlags;
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
	 * paks using the provider and options above. The caller owns async startup
	 * timing, config parsing, directory scans, embedded resource extraction,
	 * candidate validation, retries, diagnostics, and error policy.
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
