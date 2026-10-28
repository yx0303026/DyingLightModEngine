#include "ModSDK.h"

#include <Windows.h>
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
    if (hostApi == nullptr || hostApi->structSize < sizeof(DlModHostApiV1)) {
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
    runtime.hostApi = *hostApi;
    runtime.initialized = true;
    return DLMOD_RESULT_SUCCESS;
}

DlModResult MODSDK_CALL RegisterMod(const DlModDescriptorV1* mod)
{
    if (mod == nullptr || mod->structSize < sizeof(DlModDescriptorV1) || mod->abiVersion != MODSDK_ABI_VERSION ||
        mod->id == nullptr || mod->id[0] == '\0' || mod->name == nullptr || mod->version == nullptr || mod->onLoad == nullptr) {
        return DLMOD_RESULT_INVALID_ARGUMENT;
    }

    try {
        EngineRuntime& runtime = GetRuntime();
        const std::lock_guard<std::recursive_mutex> lifecycleGuard(runtime.lifecycleLock);
        DlModHostApiV1 hostApi{};
        const std::string id(mod->id);
        {
            const ExclusiveLockGuard stateGuard(runtime.lock);
            if (!runtime.initialized) {
                return DLMOD_RESULT_INVALID_STATE;
            }
            const auto existing = FindModRecord(runtime, id);
            if (existing != runtime.mods.end()) {
                return DLMOD_RESULT_INVALID_STATE;
            }
            runtime.mods.push_back({id, mod->name, mod->version, mod->context, mod->onUnload, ModState::Loading});
            hostApi = runtime.hostApi;
        }

        DlModResult loadResult = DLMOD_RESULT_INVALID_STATE;
        try {
            loadResult = mod->onLoad(mod->context, &hostApi);
        } catch (...) {
            loadResult = DLMOD_RESULT_INVALID_STATE;
        }
        {
            const ExclusiveLockGuard stateGuard(runtime.lock);
            const auto failed = FindModRecord(runtime, id);
            if (failed == runtime.mods.end()) {
                return DLMOD_RESULT_INVALID_STATE;
            }
            if (loadResult == DLMOD_RESULT_SUCCESS) {
                failed->state = ModState::Loaded;
            } else {
                runtime.mods.erase(failed);
            }
        }
        return loadResult;
    } catch (...) {
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
            removed = *existing;
            runtime.mods.erase(existing);
        }

        if (removed.onUnload != nullptr) {
            try {
                removed.onUnload(removed.context);
            } catch (...) {
            }
        }
        return DLMOD_RESULT_SUCCESS;
    } catch (...) {
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

        for (auto mod = mods.rbegin(); mod != mods.rend(); ++mod) {
            if (mod->onUnload != nullptr) {
                try {
                    mod->onUnload(mod->context);
                } catch (...) {
                }
            }
        }
        return DLMOD_RESULT_SUCCESS;
    } catch (...) {
        return DLMOD_RESULT_INVALID_STATE;
    }
}

const DlModEngineApiV1 g_engineApi = {
    sizeof(DlModEngineApiV1),
    MODSDK_ABI_VERSION,
    &Initialize,
    &RegisterMod,
    &UnregisterMod,
    &Shutdown
};

}

DlModResult MODSDK_CALL ModSDK_QueryApi(uint32_t requestedAbiVersion, DlModEngineApiV1* api)
{
    if (api == nullptr || api->structSize < sizeof(DlModEngineApiV1)) {
        return DLMOD_RESULT_INVALID_ARGUMENT;
    }
    if (requestedAbiVersion != MODSDK_ABI_VERSION) {
        return DLMOD_RESULT_UNSUPPORTED_ABI;
    }

    *api = g_engineApi;
    return DLMOD_RESULT_SUCCESS;
}
