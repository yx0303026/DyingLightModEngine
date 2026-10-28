# Custom Pak Mod

Custom Pak Mod mounts every `.pak` file directly inside the `CustomPak`
directory next to `custom_pak_mod.dll`. The filesystem module is resolved
asynchronously after the mod loads, and pak files are mounted in filename
order.

## Build

```powershell
cmake --preset x64-release
cmake --build out/build-vs2026/x64-release --config Release --target CustomPakMod
```

Use `x86-release` instead for a 32-bit game.

## Install

Install the build into the game's directory:

```powershell
cmake --install out/build-vs2026/x64-release --config Release --prefix "C:/Games/Dying Light"
```

Place pak files in this layout:

```text
Dying Light/
  Mods/
    CustomPakMod/
      custom_pak_mod.dll
      CustomPak/
        example.pak
```

When the engine is built as **Debug**, CustomPakMod logs are written to:

```text
Dying Light/Logs/CustomPakMod/YYYY-MM-DD_HH-MM-SS.log
```

Release builds keep debugger output available but do not create log files.
