#pragma once

#include <stdint.h>

#if defined(_WIN32)
#define MODSDK_CALL __cdecl
#if defined(MODSDK_BUILD)
#define MODSDK_API __declspec(dllexport)
#else
#define MODSDK_API __declspec(dllimport)
#endif
#else
#define MODSDK_CALL
#define MODSDK_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define MODSDK_ABI_VERSION 1u

	typedef enum DlModResult {
		DLMOD_RESULT_SUCCESS = 0,
		DLMOD_RESULT_INVALID_ARGUMENT = 1,
		DLMOD_RESULT_UNSUPPORTED_ABI = 2,
		DLMOD_RESULT_INVALID_STATE = 3
	} DlModResult;

	typedef void(MODSDK_CALL* DlModLogFn)(void* context, uint32_t level, const char* message);

	typedef struct DlModHostApiV1 {
		uint32_t structSize;
		uint32_t abiVersion;
		void* context;
		DlModLogFn log;
	} DlModHostApiV1;

	typedef DlModResult(MODSDK_CALL* DlModOnLoadFn)(void* modContext, const DlModHostApiV1* hostApi);
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

	typedef DlModResult(MODSDK_CALL* DlModEngineInitializeFn)(const DlModHostApiV1* hostApi);
	typedef DlModResult(MODSDK_CALL* DlModEngineRegisterModFn)(const DlModDescriptorV1* mod);
	typedef DlModResult(MODSDK_CALL* DlModEngineUnregisterModFn)(const char* modId);
	typedef DlModResult(MODSDK_CALL* DlModEngineShutdownFn)(void);

	typedef struct DlModEngineApiV1 {
		uint32_t structSize;
		uint32_t abiVersion;
		DlModEngineInitializeFn initialize;
		DlModEngineRegisterModFn registerMod;
		DlModEngineUnregisterModFn unregisterMod;
		DlModEngineShutdownFn shutdown;
	} DlModEngineApiV1;

	typedef DlModResult(MODSDK_CALL* ModSDKQueryApiFn)(uint32_t requestedAbiVersion, DlModEngineApiV1* api);

	MODSDK_API DlModResult MODSDK_CALL ModSDK_QueryApi(
		uint32_t requestedAbiVersion,
		DlModEngineApiV1* api);

#ifdef __cplusplus
}
#endif


#pragma region Logger

#ifdef __cplusplus
enum class ModSDKLogLevel : uint32_t {
	Trace = 0,
	Debug = 1,
	Info = 2,
	Warning = 3,
	Error = 4,
	Critical = 5
};

class Logger final {
public:
	static MODSDK_API void Log(ModSDKLogLevel level, const char* message) noexcept;
	static MODSDK_API void Logf(ModSDKLogLevel level, const char* format, ...) noexcept;

	static MODSDK_API void Trace(const char* message) noexcept;
	static MODSDK_API void Debug(const char* message) noexcept;
	static MODSDK_API void Info(const char* message) noexcept;
	static MODSDK_API void Warning(const char* message) noexcept;
	static MODSDK_API void Error(const char* message) noexcept;
	static MODSDK_API void Critical(const char* message) noexcept;
};
#endif

#pragma endregion


#pragma region ModManager



#pragma endregion
