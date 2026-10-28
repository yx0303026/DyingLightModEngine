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
