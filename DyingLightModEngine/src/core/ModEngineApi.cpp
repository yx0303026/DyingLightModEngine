#include "sdk/EngineApiAbi.h"
#include "runtime/EngineHookRuntime.h"
#include "runtime/EngineHotkeyRuntime.h"
#include "tracking/EngineHunterUvShieldRuntime.h"
#include "logging/EngineLogger.h"

#include <Windows.h>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace {

	enum class ModState {
		Loading,
		Loaded
	};

	struct ModRecord {
		std::string id;
		std::string name;
		std::string version;
		void* context = nullptr;
		DlModOnUnloadFn onUnload = nullptr;
		DlModOnRenderFrameFn onRenderFrame = nullptr;
		DlModIsUiOpenFn isUiOpen = nullptr;
		DlModOnWindowMessageFn onWindowMessage = nullptr;
		void* logContext = nullptr;
		ModState state = ModState::Loading;
	};

	struct EngineRuntime {
		SRWLOCK lock = SRWLOCK_INIT;
		std::recursive_mutex lifecycleLock;
		bool initialized = false;
		DlModHostApiV1 hostApi{};
		std::vector<ModRecord> mods;
	};

	EngineRuntime g_runtime;

	EngineRuntime& GetRuntime()
	{
		return g_runtime;
	}

	class ExclusiveLockGuard {
	public:
		explicit ExclusiveLockGuard(SRWLOCK& lock)
			: lock_(lock)
		{
			AcquireSRWLockExclusive(&lock_);
		}

		~ExclusiveLockGuard()
		{
			ReleaseSRWLockExclusive(&lock_);
		}

		ExclusiveLockGuard(const ExclusiveLockGuard&) = delete;
		ExclusiveLockGuard& operator=(const ExclusiveLockGuard&) = delete;

	private:
		SRWLOCK& lock_;
	};

	std::vector<ModRecord>::iterator FindModRecord(EngineRuntime& runtime, const std::string& modId)
	{
		for (auto mod = runtime.mods.begin(); mod != runtime.mods.end(); ++mod) {
			if (mod->id == modId) {
				return mod;
			}
		}
		return runtime.mods.end();
	}

	bool HasLoadingMods(const EngineRuntime& runtime)
	{
		for (const ModRecord& mod : runtime.mods) {
			if (mod.state == ModState::Loading) {
				return true;
			}
		}
		return false;
	}

	DlModResult MODSDK_CALL Initialize(const DlModHostApiV1* hostApi)
	{
		if (hostApi == nullptr ||
			hostApi->structSize < offsetof(DlModHostApiV1, getUiMouseState)) {
			return DLMOD_RESULT_INVALID_ARGUMENT;
		}
		if (hostApi->abiVersion != MODSDK_ABI_VERSION) {
			return DLMOD_RESULT_UNSUPPORTED_ABI;
		}

		EngineRuntime& runtime = GetRuntime();
		const std::lock_guard<std::recursive_mutex> lifecycleGuard(runtime.lifecycleLock);
		const ExclusiveLockGuard stateGuard(runtime.lock);
		if (runtime.initialized) {
			return DLMOD_RESULT_INVALID_STATE;
		}
		runtime.hostApi = {};
		std::memcpy(
			&runtime.hostApi,
			hostApi,
			(hostApi->structSize < sizeof(runtime.hostApi))
				? hostApi->structSize
				: sizeof(runtime.hostApi));
		runtime.hostApi.structSize =
			hostApi->structSize < sizeof(runtime.hostApi)
			? hostApi->structSize
			: sizeof(runtime.hostApi);
		runtime.initialized = true;
		return DLMOD_RESULT_SUCCESS;
	}

	DlModResult MODSDK_CALL RegisterMod(const DlModDescriptorV1* mod)
	{
		if (mod == nullptr || mod->structSize < DLMOD_DESCRIPTOR_V1_MIN_SIZE || mod->abiVersion != MODSDK_ABI_VERSION ||
			mod->id == nullptr || mod->id[0] == '\0' || mod->name == nullptr || mod->version == nullptr || mod->onLoad == nullptr) {
			return DLMOD_RESULT_INVALID_ARGUMENT;
		}

		void* logContext = nullptr;
		try {
			EngineRuntime& runtime = GetRuntime();
			const std::lock_guard<std::recursive_mutex> lifecycleGuard(runtime.lifecycleLock);
			DlModHostApiV1 hostApi{};
			const std::string id(mod->id);
			const char* logName = mod->name;
			if (mod->structSize >= sizeof(DlModDescriptorV1) && mod->logName != nullptr && mod->logName[0] != '\0') {
				logName = mod->logName;
			}
			logContext = EngineLogger::CreateModContext(logName);
			if (logContext == nullptr) {
				return DLMOD_RESULT_INVALID_ARGUMENT;
			}
			{
				const ExclusiveLockGuard stateGuard(runtime.lock);
				if (!runtime.initialized) {
					EngineLogger::DestroyModContext(logContext);
					logContext = nullptr;
					return DLMOD_RESULT_INVALID_STATE;
				}
				const auto existing = FindModRecord(runtime, id);
				if (existing != runtime.mods.end()) {
					EngineLogger::DestroyModContext(logContext);
					logContext = nullptr;
					return DLMOD_RESULT_INVALID_STATE;
				}
				const DlModOnRenderFrameFn onRenderFrame =
					mod->structSize >= sizeof(DlModDescriptorV1) ? mod->onRenderFrame : nullptr;
				const DlModIsUiOpenFn isUiOpen =
					mod->structSize >= offsetof(DlModDescriptorV1, isUiOpen) + sizeof(DlModIsUiOpenFn)
					? mod->isUiOpen
					: nullptr;
				const DlModOnWindowMessageFn onWindowMessage =
					mod->structSize >= offsetof(DlModDescriptorV1, onWindowMessage) + sizeof(DlModOnWindowMessageFn)
					? mod->onWindowMessage
					: nullptr;
				runtime.mods.push_back({
					id,
					mod->name,
					mod->version,
					mod->context,
					mod->onUnload,
					onRenderFrame,
					isUiOpen,
					onWindowMessage,
					logContext,
					ModState::Loading
				});
				hostApi = runtime.hostApi;
				hostApi.context = logContext;
				logContext = nullptr;
			}

			DlModResult loadResult = DLMOD_RESULT_INVALID_STATE;
			const std::string* previousLoadingMod =
				EngineHookRuntimeDetail::SetLoadingMod(&id);
			const std::string* previousHotkeyMod =
				EngineHotkeyRuntimeDetail::SetLoadingMod(&id);
			try {
				loadResult = mod->onLoad(mod->context, &hostApi);
			}
			catch (...) {
				loadResult = DLMOD_RESULT_INVALID_STATE;
			}
			EngineHotkeyRuntimeDetail::SetLoadingMod(previousHotkeyMod);
			EngineHookRuntimeDetail::SetLoadingMod(previousLoadingMod);
			{
				const ExclusiveLockGuard stateGuard(runtime.lock);
				const auto failed = FindModRecord(runtime, id);
				if (failed == runtime.mods.end()) {
					return DLMOD_RESULT_INVALID_STATE;
				}
				if (loadResult == DLMOD_RESULT_SUCCESS) {
					failed->state = ModState::Loaded;
				}
				else {
					if (!EngineHookRuntime::RemoveForMod(id)) {
						EngineHookRuntime::RemoveAll();
					}
					EngineHotkeyRuntime::RemoveForOwner(id);
					EngineHunterUvShieldRuntime::UnregisterHooks();
					EngineLogger::DestroyModContext(failed->logContext);
					logContext = nullptr;
					runtime.mods.erase(failed);
				}
			}
			return loadResult;
		}
		catch (...) {
			EngineLogger::DestroyModContext(logContext);
			return DLMOD_RESULT_INVALID_STATE;
		}
	}

	DlModResult MODSDK_CALL UnregisterMod(const char* modId)
	{
		if (modId == nullptr || modId[0] == '\0') {
			return DLMOD_RESULT_INVALID_ARGUMENT;
		}

		try {
			EngineRuntime& runtime = GetRuntime();
			const std::lock_guard<std::recursive_mutex> lifecycleGuard(runtime.lifecycleLock);
			ModRecord removed{};
			{
				const ExclusiveLockGuard stateGuard(runtime.lock);
				if (!runtime.initialized) {
					return DLMOD_RESULT_INVALID_STATE;
				}
				const auto existing = FindModRecord(runtime, modId);
				if (existing == runtime.mods.end()) {
					return DLMOD_RESULT_INVALID_ARGUMENT;
				}
				if (existing->state != ModState::Loaded) {
					return DLMOD_RESULT_INVALID_STATE;
				}
				if (!EngineHookRuntime::RemoveForMod(existing->id)) {
					return DLMOD_RESULT_INVALID_STATE;
				}
				EngineHotkeyRuntime::RemoveForOwner(existing->id);
				EngineHunterUvShieldRuntime::UnregisterHooks();
				removed = *existing;
				runtime.mods.erase(existing);
			}

			if (removed.onUnload != nullptr) {
				try {
					removed.onUnload(removed.context);
				}
				catch (...) {
				}
			}
			EngineLogger::DestroyModContext(removed.logContext);
			return DLMOD_RESULT_SUCCESS;
		}
		catch (...) {
			return DLMOD_RESULT_INVALID_STATE;
		}
	}

	DlModResult MODSDK_CALL Shutdown()
	{
		try {
			EngineRuntime& runtime = GetRuntime();
			const std::lock_guard<std::recursive_mutex> lifecycleGuard(runtime.lifecycleLock);
			std::vector<ModRecord> mods;
			{
				const ExclusiveLockGuard stateGuard(runtime.lock);
				if (!runtime.initialized || HasLoadingMods(runtime)) {
					return DLMOD_RESULT_INVALID_STATE;
				}
				runtime.initialized = false;
				mods.swap(runtime.mods);
				runtime.hostApi = {};
			}

			EngineHookRuntime::RemoveAll();
			for (auto mod = mods.rbegin(); mod != mods.rend(); ++mod) {
				if (mod->onUnload != nullptr) {
					try {
						mod->onUnload(mod->context);
					}
					catch (...) {
					}
				}
				EngineLogger::DestroyModContext(mod->logContext);
			}
			return DLMOD_RESULT_SUCCESS;
		}
		catch (...) {
			return DLMOD_RESULT_INVALID_STATE;
		}
	}

	void MODSDK_CALL RenderFrame(void* graphicsContext)
	{
		EngineRuntime& runtime = GetRuntime();
		const std::lock_guard<std::recursive_mutex> lifecycleGuard(runtime.lifecycleLock);
		std::vector<ModRecord> mods;
		{
			const ExclusiveLockGuard stateGuard(runtime.lock);
			if (!runtime.initialized) {
				return;
			}
			mods = runtime.mods;
		}

		for (const ModRecord& mod : mods) {
			if (mod.state != ModState::Loaded || mod.onRenderFrame == nullptr) {
				continue;
			}
			try {
				mod.onRenderFrame(mod.context, graphicsContext);
			}
			catch (...) {
			}
		}
	}

	uint8_t MODSDK_CALL IsUiOpen()
	{
		EngineRuntime& runtime = GetRuntime();
		const std::lock_guard<std::recursive_mutex> lifecycleGuard(runtime.lifecycleLock);
		std::vector<ModRecord> mods;
		{
			const ExclusiveLockGuard stateGuard(runtime.lock);
			if (!runtime.initialized) {
				return 0U;
			}
			mods = runtime.mods;
		}

		for (const ModRecord& mod : mods) {
			if (mod.state != ModState::Loaded || mod.isUiOpen == nullptr) {
				continue;
			}
			try {
				if (mod.isUiOpen(mod.context) != 0U) {
					return 1U;
				}
			}
			catch (...) {
			}
		}
		return 0U;
	}

	void MODSDK_CALL OnWindowMessage(
		uintptr_t window,
		uint32_t message,
		uintptr_t wParam,
		intptr_t lParam)
	{
		EngineRuntime& runtime = GetRuntime();
		const std::lock_guard<std::recursive_mutex> lifecycleGuard(runtime.lifecycleLock);
		std::vector<ModRecord> mods;
		{
			const ExclusiveLockGuard stateGuard(runtime.lock);
			if (!runtime.initialized) {
				return;
			}
			mods = runtime.mods;
		}

		for (const ModRecord& mod : mods) {
			if (mod.state != ModState::Loaded || mod.onWindowMessage == nullptr) {
				continue;
			}
			try {
				mod.onWindowMessage(mod.context, window, message, wParam, lParam);
			}
			catch (...) {
			}
		}
	}

	const ModEngineApiV1 g_engineApi = {
		sizeof(ModEngineApiV1),
		MODSDK_ABI_VERSION,
		&Initialize,
		&RegisterMod,
		&UnregisterMod,
		&Shutdown,
		&RenderFrame,
		&IsUiOpen,
		&OnWindowMessage
	};

}

DlModResult MODSDK_CALL ModSDK_QueryApi(uint32_t requestedAbiVersion, ModEngineApiV1* api)
{
	if (api == nullptr || api->structSize < sizeof(ModEngineApiV1)) {
		return DLMOD_RESULT_INVALID_ARGUMENT;
	}
	if (requestedAbiVersion != MODSDK_ABI_VERSION) {
		return DLMOD_RESULT_UNSUPPORTED_ABI;
	}

	*api = g_engineApi;
	return DLMOD_RESULT_SUCCESS;
}

