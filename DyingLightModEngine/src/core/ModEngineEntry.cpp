#include <Windows.h>

#include "core/EngineHost.h"

namespace {

	INIT_ONCE g_initializeOnce = INIT_ONCE_STATIC_INIT;
	HMODULE g_moduleHandle = nullptr;
	HRESULT g_initializationResult = E_FAIL;

	BOOL CALLBACK InitializeEngineOnce(PINIT_ONCE, PVOID, PVOID*)
	{
		g_initializationResult = InitializeModEngine(g_moduleHandle) ? S_OK : E_FAIL;
		return SUCCEEDED(g_initializationResult);
	}

}

extern "C" BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH) {
		g_moduleHandle = instance;
		DisableThreadLibraryCalls(instance);
	}
	return TRUE;
}

extern "C" HRESULT WINAPI ModEngineInitialize()
{
	if (!InitOnceExecuteOnce(&g_initializeOnce, &InitializeEngineOnce, nullptr, nullptr)) {
		return E_FAIL;
	}
	return g_initializationResult;
}

extern "C" HRESULT WINAPI ModEngineShutdown()
{
	return ShutdownModEngine() ? S_OK : E_FAIL;
}
