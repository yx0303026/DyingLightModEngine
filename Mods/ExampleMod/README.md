# Example Mod

Example Mod is a small loadable mod for Dying Light Mod Engine. It demonstrates
the Mod SDK descriptor, ABI version check, host logging callback, and load and
unload callbacks.

## Build

Configure the repository with a preset matching the target game installation:

```powershell
cmake --preset x64-release
cmake --build out/build-vs2026/x64-release --config Release --target ExampleMod
```

Use `x86-release` instead for a 32-bit game. The DLL is written to the
architecture-specific `build` directory.

## Install

Install the built mod into the game's directory, which contains the game
executable:

```powershell
cmake --install out/build-vs2026/x64-release --config Release --prefix "C:/Games/Dying Light"
```

The installed layout is:

```text
Dying Light/
  Mods/
    ExampleMod/
      example_mod.dll
```

The engine discovers the DLL when loading all mods, or when loading the mod by
its folder name `ExampleMod`. On load and unload, the mod writes an informational
message through the host-provided logging callback.

## Shared Hooks

The host can provide `hostApi->hooks`. A mod can register a detour during
`onLoad`; the engine owns the target hook and chains registrations that use the
same target address:

```cpp
using UpdateFn = void(__fastcall*)(void* self, float deltaSeconds);

UpdateFn g_originalUpdate = nullptr;

void __fastcall UpdateDetour(void* self, float deltaSeconds)
{
	// 模组逻辑
	g_originalUpdate(self, deltaSeconds);
}

DlModResult RegisterUpdateHook(const DlModHostApiV1* hostApi, void* target)
{
	if (hostApi == nullptr ||
		hostApi->structSize < DLMOD_HOST_API_V1_HOOKS_SIZE ||
		hostApi->hooks == nullptr ||
		hostApi->hooks->structSize < DLMOD_HOOK_API_V1_MIN_SIZE ||
		hostApi->hooks->abiVersion != MODSDK_ABI_VERSION) {
		return DLMOD_RESULT_UNSUPPORTED_FEATURE;
	}

	return hostApi->hooks->registerHook(
		target,
		reinterpret_cast<void*>(&UpdateDetour),
		reinterpret_cast<void**>(&g_originalUpdate));
}
```

All detours for one address must use the same function signature and calling
convention. Each detour must call its `original` function when it wants the
remaining chain and game implementation to run. Registration is limited to
`onLoad`, and the engine removes a mod's registrations before unloading its DLL.
Hooks installed directly through MinHook or raw byte patches do not participate
in this registry and cannot be merged automatically.
