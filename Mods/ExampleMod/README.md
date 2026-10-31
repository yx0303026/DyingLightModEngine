# Example Mod

Example Mod is a small loadable mod for Dying Light Mod Engine. It demonstrates
the Mod SDK descriptor, ABI version check, host logging callback, load and
unload callbacks, per-frame rendering callbacks, the entity API, and the
game-native hotkey API.

## UV Shield Skill (Be the Zombie)

On load the mod installs the engine's hunter UV shield hooks and turns the Night
Hunter's UV immunity into a toggleable skill:

- Press **F7** to toggle the shield by default. The hotkey is also exposed in
  the game's native key binding settings, where it can be rebound.
- The charge number is hidden using the game's unlimited-charge sentinel.
- The HUD progress ring tracks shield energy instead of the hunter's UV resistance.
- A full energy bar keeps the shield up for **6 seconds**.
- While the shield is off the bar recharges from empty to full in **20 seconds**.
- When energy reaches zero the shield switches off automatically and recharges.

The mod drives the engine through `DlModEntityApiV1`:

```cpp
entity->installUvShieldTracking();
entity->setHunterUvShieldCharges(-2);
entity->setHunterUvShieldProgress(energy);
entity->setHunterUvShield(DLMOD_HUNTER_UV_SHIELD_MODE_PIN, energy * 6.0F);
entity->setHunterUvShield(DLMOD_HUNTER_UV_SHIELD_MODE_OFF, 0.0F);
```

Energy is only consumed or refilled while a Be the Zombie hunter is active, so
the campaign is unaffected.

## Native Hotkeys

The example registers its toggle through `DlModHotkeyApiV1`. The engine adds the
hotkey to the game's native input settings page and keeps the game binding as
the source of truth:

```cpp
DlModHotkeyDescV1 hotkey{};
hotkey.structSize = sizeof(hotkey);
hotkey.abiVersion = DLMOD_HOTKEY_ABI_VERSION;
hotkey.id = "example.mod.toggle_uv_shield";
hotkey.displayName = "Toggle UV Shield";
hotkey.pageTitle = "Example Mod";
hotkey.defaultKey = DLMOD_HOTKEY_KEY_F7;
hotkey.device = DLMOD_HOTKEY_DEVICE_INFER;
hotkey.userData = &state;
hotkey.callback = &OnShieldHotkey;

hostApi->hotkey->registerHotkey(&hotkey);
```

Call `registerHotkey` from `onLoad`. The engine creates an `Example Mod` page
in the game's native key binding settings and adds one row named
`Toggle UV Shield`. The user can rebind that row in the game; the runtime
refreshes the binding after the game's input configuration reloads. The
callback fires on the key-down edge with the action ID allocated from the
game's free action slots. `getHotkeyKey` returns the current effective binding,
and `unregisterHotkey` must be called from `onUnload` so the callback never
points at an unloaded DLL.

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
